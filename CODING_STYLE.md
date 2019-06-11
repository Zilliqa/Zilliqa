# Zilliqa Coding Style
<!--Source: https://github.com/ethereum/aleth/blob/master/CODING_STYLE.md -->
## Code Formatting

Use clang-format tool to format your changes. See [CONTRIBUTING](CONTRIBUTING.md) for details.


## Namespaces

1. No `using namespace` declarations in header files.

    ```cpp
           // WRONG:
           #include <cassert>
           using namespace std;
           tuple<float, float> meanAndSigma(vector<float> const& _v);

           // CORRECT:
           #include <cassert>
           std::tuple<float, float> meanAndSigma(std::vector<float> const& _v);
    ```
2. Preprocessor symbols should be prefixed with the namespace in all-caps and an underscore.

## Preprocessor

1. File comment is always at top, and includes:
   - Copyright.
   - License.
   
2. Use Zilliqa Standardized Guard for header files which is given by https://google.github.io/styleguide/cppguide.html#The__define_Guard.
    ```
      ZILLIQA_<path_to_header_file>_H_
    ```
3. Prefer static const variable to value macros.
4. Prefer inline constexpr functions to function macros.


## Capitalization

GOLDEN RULE: Preprocessor: ALL_CAPS; C++: camelCase.

1. Use camelCase for all class/structure names.
2. All preprocessor symbols (macros, macro arguments) in full uppercase with underscore word separation.


All other entities' first alpha is lower case.


## Variable Prefixes

1. Leading `m_` to data members.
2. Leading `g_` to global (non-const) variables.
3. Leading `s_` to static (non-const, non-global) variables.


## Declarations

1. {Typename} + {qualifiers} + {name}.
2. Only one per line.
3. Favour declarations close to use; don't habitually declare at top of scope ala C.
4. Always pass non-trivial parameters with a const& prefix.
5. To return multiple "out" values, prefer returning a tuple or struct.
   See [F.21].
6. Never use a macro where adequate non-preprocessor C++ can be written.
7. Make use of `auto` whenever type is clear or unimportant:
   - Always avoid doubly-stating the type.
   - Use to avoid vast and unimportant type declarations.
   - However, avoid using auto where type is not immediately obvious from the context, and especially not for arithmetic expressions.
8. If you need to pass more than one boolean to a function, consider using an enum instead
9. Prefer `enum class` to straight `enum`.
10. Try to use uniform initialization syntax wherever possible.

    ```cpp
           // WRONG:
           const double d = 0;
           int i, j;
           char *s;
           float meanAndSigma(std::vector<float> _v, float* _sigma, bool _approximate);
           Derived* x(dynamic_cast<Derived*>(base));
           for (map<ComplexTypeOne, ComplexTypeTwo>::iterator i = l.begin(); i != l.end(); ++l) {}

           // CORRECT:
           enum class Accuracy
           {
               Approximate,
               Exact
           };
           double const d = 0;
           int i;
           int j;
           char* s;
           std::tuple<float, float> meanAndSigma(std::vector<float> const& _v, Accuracy _a);
           auto x = dynamic_cast<Derived*>(base);
           for (auto i = x.begin(); i != x.end(); ++i) {}
    ```

## Structs & Classes

1. Structs to be used when all members public and no virtual functions.
2. Classes to be used in all other circumstances.



## Members

1. One member per line only.
2. Private, non-static, non-const fields prefixed with m_.
3. Avoid public fields, except in structs.
4. Use `override`, `final` and `const` as much as possible.
5. No implementations with the class declaration, except:
   - template or force-inline method (though prefer implementation at bottom of header file).
   - one-line implementation (in which case include it in same line as declaration).
6. For a property `foo`
   - Member: `m_foo`;
   - Getter: `foo()`; also: for booleans, `isFoo()`
   - Setter: `setFoo()`;


## Naming

1. Collection conventions:
   - `...s` means `std::vector` e.g. `using MyTypes = std::vector<MyType>`
   - `...Set` means `std::set` e.g. `using MyTypeSet = std::set<MyType>`
   - `...Hash` means `std::unordered_set` e.g. `using MyTypeHash = std::unordered_set<MyType>`
2. Class conventions:
   - `...Face` means the interface of some shared concept. (e.g. `FooFace` might be a pure virtual class.)
3. Avoid unpronounceable names:
   - If you need to shorten a name favour a pronouncable slice of the original to a scattered set of consonants.
   - e.g. `Manager` shortens to `Man` rather than `Mgr`.
4. Avoid prefixes of initials (e.g. DON'T use `IMyInterface`, `CMyImplementation`)
5. Find short, memorable & (at least semi-) descriptive names for commonly used classes or name-fragments.
   - A dictionary and thesaurus are your friends.
   - Spell correctly.
   - Think carefully about the class's purpose.
   - Imagine it as an isolated component to try to decontextualise it when considering its name.
   - Don't be trapped into naming it (purely) in terms of its implementation.



## Type-Definitions

1. Prefer `using` to `typedef`. E.g. `using ints = std::vector<int>` rather than
   `typedef std::vector<int> ints`.
2. Generally avoid shortening a standard form that already includes all important information:
   - e.g. stick to `shared_ptr<X>` rather than shortening to `ptr<X>`.
3. In general expressions should be roughly as important/semantically meaningful as the space they occupy.




## Logging
- Use `INFO` for non-critical, informative logging messages
- Use `WARNING` for logging potentially dangerous messages
- Use `FATAL` for logging a message and thereafter terminating the process


## Recommended Reading

Herb Sutter and Bjarne Stroustrup
- "C++ Core Guidelines" (https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md)

Herb Sutter and Andrei Alexandrescu
- "C++ Coding Standards: 101 Rules, Guidelines, and Best Practices"

Scott Meyers
- "Effective C++: 55 Specific Ways to Improve Your Programs and Designs (3rd Edition)"
- "More Effective C++: 35 New Ways to Improve Your Programs and Designs"
- "Effective Modern C++: 42 Specific Ways to Improve Your Use of C++11 and C++14"


[F.21]: https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md#Rf-out-multi

