// MISRA C++:2023 Rule 8.2.6 (Required): static_cast shall not be used to
// perform a downcast from a base to a derived class.
// Expected diagnostic: cppcoreguidelines-pro-type-static-cast-downcast

class TvsDownBase {
public:
    virtual ~TvsDownBase() = default;
    int value = 0;
};

class TvsDownDerived : public TvsDownBase {
public:
    int extra = 42;
};

int tvs_static_downcast(TvsDownBase *base) {
    auto *d = static_cast<TvsDownDerived *>(base);  // Static downcast — target violation
    return d->extra;
}
