#pragma once

// An indirection (CONCAT calling CONCAT_INNER) is needed to expand the macro __COUNTER__
#define CONCAT(a,b) CONCAT_INNER(a,b)
#define CONCAT_INNER(a,b) a##b
#define UNIQUE_ID(x) CONCAT(_unique_##x,__COUNTER__)

/**
   DEFER macro, inspired by https://twitter.com/molecularmusing/status/1434784711759568897/photo/1
   Used to defer execution of a block to the end of the scope.
   Usage:

   int main()
   {
       DEFER
       {
           logger::info("This will be printed second, after `return 0`.\n");
       };

       logger::info("This will be printed first.\n");

       return 0;
   }
**/

template<typename T>
struct [[nodiscard]] DeferrableFunction
{
    DeferrableFunction(T &&_closure) noexcept
        : closure{std::move(_closure)}
    {
    }

    DeferrableFunction(const DeferrableFunction &other) = delete;
    DeferrableFunction(DeferrableFunction &&other) = delete;

    ~DeferrableFunction() noexcept
    {
        closure();
    }

    T closure;
};

template<class EF>
DeferrableFunction(EF) -> DeferrableFunction<EF>;

#define DEFER const DeferrableFunction UNIQUE_ID(defer) = [&]()
