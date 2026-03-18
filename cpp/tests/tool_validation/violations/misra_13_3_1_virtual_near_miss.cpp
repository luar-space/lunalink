// MISRA C++:2023 Rule 13.3.1 (Required): Virtual function signatures must
// match exactly; near-misses indicate bugs.
// Expected diagnostic: bugprone-virtual-near-miss

class TvsNearMissBase {
public:
    virtual ~TvsNearMissBase() = default;
    virtual int compute(int x) { return x; }
};

class TvsNearMissDerived : public TvsNearMissBase {
public:
    virtual int computee(int x) { return x + 1; }  // Typo near miss — target violation
};
