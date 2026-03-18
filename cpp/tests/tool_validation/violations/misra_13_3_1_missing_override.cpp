// MISRA C++:2023 Rule 13.3.1 (Required): An overriding function shall be
// declared with the override or final specifier.
// Expected diagnostic: modernize-use-override

class TvsBase {
public:
    virtual ~TvsBase() = default;
    virtual int compute(int x) { return x; }
};

class TvsDerived : public TvsBase {
public:
    int compute(int x) { return x + 1; }  // Missing override — target violation
};
