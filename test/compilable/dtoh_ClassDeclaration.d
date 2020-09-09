/*
REQUIRED_ARGS: -HC -c -o-
PERMUTE_ARGS:
TEST_OUTPUT:
---
// Automatically generated by Digital Mars D Compiler

#pragma once

#include <stddef.h>
#include <stdint.h>


class C;
class A;
struct Inner;

class C
{
public:
    int8_t a;
    int32_t b;
    int64_t c;
};

class C2
{
public:
    int32_t a;
    int32_t b;
    int64_t c;
    C2(int32_t a);
};

class Aligned
{
public:
    int8_t a;
    int32_t b;
    int64_t c;
    Aligned(int32_t a);
};

class A
{
public:
    int32_t a;
    C* c;
    virtual void foo();
    extern "C" virtual void bar();
    virtual void baz(int32_t x = 42);
    struct
    {
        int32_t x;
        int32_t y;
    };
    union
    {
        int32_t u1;
        char u2[4$?:32=u|64=LLU$];
    };
    struct Inner
    {
        int32_t x;
        Inner() :
            x()
        {
        }
    };

    class InnerC
    {
    public:
        int32_t x;
    };

    class NonStaticInnerC
    {
    public:
        int32_t x;
        A* this;
    };

    typedef Inner I;
    class CC;

};
---
*/

/*
ClassDeclaration has the following issues:
  * align(n) does nothing. You can use align on classes in C++, though It is generally regarded as bad practice and should be avoided
*/

extern (C++) class C
{
    byte a;
    int b;
    long c;
}

extern (C++) class C2
{
    int a = 42;
    int b;
    long c;

    this(int a) {}
}

extern (C) class C3
{
    int a = 42;
    int b;
    long c;

    this(int a) {}
}

extern (C++) align(1) class Aligned
{
    byte a;
    int b;
    long c;

    this(int a) {}
}

extern (C++) class A
{
    int a;
    C c;

    void foo();
    extern (C) void bar() {}
    extern (C++) void baz(int x = 42) {}

    struct
    {
        int x;
        int y;
    }

    union
    {
        int u1;
        char[4] u2;
    }

    struct Inner
    {
        int x;
    }

    static extern(C++) class InnerC
    {
        int x;
    }

    class NonStaticInnerC
    {
        int x;
    }

    alias I = Inner;

    extern(C++) class CC;

}
