#include "pch.h"

#include <mutex>

#include "convar.hh"
#include "sdk.hh"

// Helper to prevent crashes if Cvar isnt initied properly yet
bool can_init_convars_at_construction_time = false;

class IConVar;

using ChangeCallbackFn = void (*)(IConVar *, const char *, float);

static auto dll_identifier = -1;

extern Cvar* cvar;

class ConCommandBase {
public:
    ConCommandBase() : name(nullptr), value_string(nullptr), help_string(nullptr) {}
    ConCommandBase(const char *name, const char *help_string, u32 flags = 0) {
        create_base(name, help_string, flags);
    }
    virtual ~ConCommandBase() {
        cvar->unregister_command(this);
    }

    virtual bool is_command() const { return false; }

    virtual bool has_flag(int flag) const { return (flags & flag); }
    virtual void add_flag(int new_flags) { flags |= new_flags; }

    virtual const char *get_name() const { return name; }
    virtual const char *get_help_text() const { return help_string; }

    virtual bool is_registered() const {
        return registered;
    }

    virtual int get_dll_identifier() const {
        if (dll_identifier == -1) dll_identifier = cvar->allocate_dll_identifier();
        return dll_identifier;
    }

    virtual void create_base(const char *name, const char *help_string, int flags = 0) {
        assert(name);
        assert(help_string);

        registered = false;

        this->name        = name;
        this->help_string = help_string;
        this->flags       = flags;

        next = head;
        head = this;

        // We might not have Cvar here (global variables)

        if (auto cvarr = cvar && can_init_convars_at_construction_time) {
            cvar->register_command(this);
        }
    }

    virtual bool init() {
        cvar->register_command(this);
        return true;
    }

    // Convar vtable items
    virtual void set_value(const char *value) {
        assert(parent == this); // Only valid for root convars.

        float old_value = value_float;

        float new_value;
        if (value == nullptr)
            new_value = 0.0f;
        else
            new_value = (float)atof(value);

        // if we need to clamp this value then swap the original string
        // out with a temp one
        auto new_string_value = value;
        char temp_value[32];
        if (clamp_value(new_value) == true) {
            snprintf(temp_value, sizeof(temp_value), "%f", new_value);
            new_string_value = temp_value;
        }

        // Redetermine value
        value_float = new_value;
        value_int   = static_cast<int>(value_float);

        // TODO: do we need to handle never as string convars??
        change_string_value(new_string_value, old_value);
    }
    virtual void set_value(float new_value) {
        assert(parent == this);

        clamp_value(new_value);

        auto old_value = value_float;
        value_float    = new_value;
        value_int      = static_cast<int>(new_value);

        char temp_value[32];
        snprintf(temp_value, sizeof(temp_value), "%f", new_value);
        change_string_value(temp_value, old_value);
    }
    virtual void set_value(int new_value) {
        return set_value(static_cast<float>(new_value));
    }

    virtual void internal_set_value(const char *new_value) {
        return set_value(new_value);
    }
    virtual void internal_set_value(float new_value) {
        return set_value(new_value);
    }
    virtual void internal_set_value(int new_value) {
        return set_value(new_value);
    }

    virtual bool clamp_value(float &value) {
        if (has_min && (value < value_min)) {
            value = value_min;
            return true;
        }

        if (has_max && (value > value_max)) {
            value = value_max;
            return true;
        }

        return false;
    }

    virtual void change_string_value(const char *new_value, float old_value) {
        if (value_string != nullptr) {
            delete[] value_string;
            value_string = nullptr;
        }

        auto new_len = strlen(new_value) + 1;
        value_string = new char[new_len];

        strcpy_s(value_string, new_len, new_value);
        value_string_length = new_len;

        if (change_callback != nullptr) change_callback(to_iconvar(), new_value, old_value);
    }

    // helper functions for converting from IConVar and to IConVar
    static auto from_iconvar(IConVar *v) { return reinterpret_cast<ConCommandBase *>(reinterpret_cast<u8 *>(v) - 24); }
    static auto to_iconvar(ConCommandBase *b) { return reinterpret_cast<IConVar *>(reinterpret_cast<u8 *>(b) + 24); }
    IConVar *   to_iconvar() { return ConCommandBase::to_iconvar(this); }

#define DEFINE_THUNK(type, name, real_name)                        \
    static void __fastcall name(IConVar *ecx, void *edx, type v) { \
        auto *real = ConCommandBase::from_iconvar(ecx);            \
        real->real_name(v);                                        \
    }
    DEFINE_THUNK(const char *, set_value_string_thunk, set_value);
    DEFINE_THUNK(float, set_value_float_thunk, set_value);
    DEFINE_THUNK(int, set_value_int_thunk, set_value);
#undef DEFINE_THUNK

    // It doesnt look like IConVar::GetName and IConVar::IsFlagSet are called
    static void __fastcall undefined_thunk(u8 *ecx, void *edx, int arg1) { assert(0); }

    virtual void create_convar(char const *name, char const *default_value, u32 flags, char const *help_string,
                               bool has_min, float min, bool has_max, float max, ChangeCallbackFn change_callback) {
        create_base(name, help_string, flags);

        {
            // Set up the MI vtables properly
            // IConvar.h
            /*
			virtual void SetValue( const char *pValue ) = 0;
			virtual void SetValue( float flValue ) = 0;
			virtual void SetValue( int nValue ) = 0;
			virtual const char *GetName( void ) const = 0;
			virtual bool IsFlagSet( int nFlag ) const = 0;
			*/

            // These all need to be properly thunked
            static auto iconvar_vtable = []() {
                auto ret = new void *[5];
                ret[0]   = (void *)&set_value_int_thunk;
                ret[1]   = (void *)&set_value_float_thunk;
                ret[2]   = (void *)&set_value_string_thunk;

                ret[3] = (void *)&undefined_thunk;
                ret[4] = (void *)&undefined_thunk;

                return ret;
            }();

            this->convar_vtable = iconvar_vtable;
        }

        parent = this;

        this->default_value = (default_value == nullptr) || (default_value[0] == '\0') ? "0.0" : default_value;

        this->has_min   = has_min;
        this->value_min = min;

        this->has_max   = has_max;
        this->value_max = max;

        this->change_callback = change_callback;

        set_value(this->default_value);
    }

public:
    ConCommandBase *next;

    bool registered;

    const char *name;
    const char *help_string;

    u32 flags;

    static ConCommandBase *head;

    // Convar is an mi class and therefore needs this
    // TODO: is this windows only - what is the linux equivilent??
    void *convar_vtable; // Should be at 0x24

    // Convar members
    ConCommandBase *parent;

    // Static data
    const char *default_value;

    // Value
    // Dynamically allocated
    char *value_string;
    int   value_string_length;

    // Values
    float value_float;
    int   value_int;

    // Min/Max values
    bool  has_min;
    float value_min;
    bool  has_max;
    float value_max;

    ChangeCallbackFn change_callback;
};

ConCommandBase *ConCommandBase::head;

const ConvarBase *ConvarBase::head = nullptr;

void ConvarBase::tf_convar_changed(IConVar *iconvar, const char *old_string, float old_float) {
	return;
    auto convar = ConCommandBase::from_iconvar(iconvar);
    assert(convar);

    for (auto c : ConvarBase::get_range()) {
        if (c->tf_convar == convar) {
            if (convar->registered == false) return;
            if (c->init_complete == false) return;

            auto modifiable  = const_cast<ConvarBase *>(c);
            auto was_clamped = modifiable->from_string(convar->value_string);

            // Remove the callback when we call set_value to prevent recursion
            // TODO: there is probably a better way to do this...
            auto callback_backup    = convar->change_callback;
            convar->change_callback = nullptr;
            if (was_clamped) convar->set_value(modifiable->to_string());
            convar->change_callback = callback_backup;

            //logging::msg("Updated convar %s to '%s' (%s)", convar->get_name(), convar->value_string, was_clamped ? "clamped" : "not clamped");
        }
    }
}

ConvarBase::ConvarBase(const char *name, ConvarType type, const ConvarBase *parent) : init_complete(false) {
    this->next = head;
    head       = this;

    this->parent = parent;
    this->t      = type;

    strcpy_s(internal_name, name);

    // Create a tf convar based on this one
    tf_convar = new ConCommandBase;
    tf_convar->create_convar(name, "", 0, name, false, 0, false, 0, &ConvarBase::tf_convar_changed);

    init_complete = true;
}

ConvarBase::~ConvarBase() {

    if (this == head) head = this->next;

    for (auto c : ConvarBase::get_range()) {
        auto modifiable = const_cast<ConvarBase *>(c);
        if (c->next == this) modifiable->next = this->next;
    }

    // Cleanup tf_convar
    // This unregisters itself
    delete this->tf_convar;
}

void ConvarBase::init_all() {
    //assert(iface::cvar);

    can_init_convars_at_construction_time = true;

    // We have to do this goofy loop here as register_command() will
    // change the `next` pointer to the next in its chain
    // which will cause all kinds of problems for us

    // TODO: this could also be fixed by using the ConvarBase linked list...

    auto c = ConCommandBase::head;
    while (c != nullptr) {
        auto next = c->next;

        cvar->register_command(c);

        c = next;
    }

    for (auto c : ConvarBase::Convar_Range()) {
        c->tf_convar->set_value(c->to_string());
    }
}

// TODO: this needs a better method of keeping in sync with the
// Interface declaration in doghook.cc
/*Cvar *get_or_init_cvar() {
    if (cvar.get() == nullptr) {
        cvar.set_from_interface("vstdlib", "VEngineCvar");
    }

    return cvar.get();
}*/

ConvarWrapper::ConvarWrapper(const char *name) {
    assert(name);
    base = cvar->find_var(name);
    assert(base);
}

int ConvarWrapper::get_int() {
    return base->value_int;
}

float ConvarWrapper::get_float() {
    return base->value_float;
}

bool ConvarWrapper::get_bool() {
    return !!base->value_int;
}

const char *ConvarWrapper::get_string() {
    return base->value_string;
}

u32 ConvarWrapper::flags() {
    return base->flags;
}

void ConvarWrapper::set_flags(u32 new_flags) {
    base->flags = new_flags;
}

const char *ConvarWrapper::defualt_value() {
    return base->default_value;
}

void ConvarWrapper::set_value(int v) {
    base->set_value(v);
}

void ConvarWrapper::set_value(float v) {
    base->set_value(v);
}

void ConvarWrapper::set_value(const char *v) {
    base->set_value(v);
}

ConvarWrapper ConvarWrapper::Range::Iterator::operator++() {
    current = current->next;
    return ConvarWrapper(const_cast<ConCommandBase *>(current));
}

ConvarWrapper::Range::Iterator ConvarWrapper::Range::begin() const { return Iterator(cvar->root_node()); }
