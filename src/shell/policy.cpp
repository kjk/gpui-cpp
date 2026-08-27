#include "shell/policy.h"

namespace gpui {

struct PolicyShared {
    uint32_t refs = 1;
    shell::Storage local{true};
    shell::Storage session{false};
};

struct Policy {
    uint32_t refs = 1;
    Capabilities capabilities;
    PolicyShared* shared = nullptr;
};

static PolicyShared* SharedRetain(PolicyShared* shared) {
    if (shared) shared->refs++;
    return shared;
}

static void SharedRelease(PolicyShared* shared) {
    if (shared && --shared->refs == 0) delete shared;
}

Policy* PolicyNew() {
    Policy* policy = new Policy();
    policy->shared = new PolicyShared();
    return policy;
}

Policy* PolicyNew(const Capabilities& capabilities) {
    Policy* policy = PolicyNew();
    policy->capabilities = capabilities;
    return policy;
}

Policy* PolicyRetain(Policy* policy) {
    if (policy) policy->refs++;
    return policy;
}

void PolicyRelease(Policy* policy) {
    if (!policy || --policy->refs != 0) return;
    SharedRelease(policy->shared);
    delete policy;
}

const Capabilities& PolicyCapabilities(const Policy* policy) {
    static const Capabilities denied;
    return policy ? policy->capabilities : denied;
}

static thread_local Policy* gDefaultPolicy = nullptr;

static Policy* EnsureDefault() {
    if (!gDefaultPolicy) gDefaultPolicy = PolicyNew();
    return gDefaultPolicy;
}

Policy* PolicyDefault() {
    return PolicyRetain(EnsureDefault());
}

void PolicySetDefault(Policy* policy) {
    Policy* replacement = policy ? PolicyRetain(policy) : PolicyNew();
    PolicyRelease(gDefaultPolicy);
    gDefaultPolicy = replacement;
}

void PolicyUpdateDefaultCapabilities(const Capabilities& capabilities) {
    Policy* current = EnsureDefault();
    Policy* replacement = new Policy();
    replacement->capabilities = capabilities;
    replacement->shared = SharedRetain(current->shared);
    gDefaultPolicy = replacement;
    PolicyRelease(current);
}

shell::Storage* PolicyStorage(Policy* policy, bool session) {
    if (!policy || !policy->shared) return nullptr;
    return session ? &policy->shared->session : &policy->shared->local;
}

bool PolicySetStoragePath(Policy* policy, Str path, Str* error) {
    shell::Storage* storage = PolicyStorage(policy, false);
    return storage && storage->SetPath(path, error);
}

bool ShellSetStoragePath(Str path, Str* error) {
    Policy* policy = PolicyDefault();
    bool ok = PolicySetStoragePath(policy, path, error);
    PolicyRelease(policy);
    return ok;
}

} // namespace gpui
