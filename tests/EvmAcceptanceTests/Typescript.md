# Typescript Quick Start Guide

This page contains short examples of typescript.

## Types

### any type

You can assign anything to it:

```typescript
let x = 1; // Its any type by default
x = "salam";

let y: any = 2; // Explicit type specification

function square(num) {
  // num has any type
  return num * num;
}
```

### Type specification

```typescript
function square(num: number) {
  return num * num;
}

// Other way to define a function. (A callable object)
const greet = (person: string) => {
  console.log(`Hello ${person}`);
};
```

### Function Return Type

```typescript
const greet = (person: string): string => {
  return `Hello ${person}`;
};
```

### Object Type

```typescript
let coordinate: {x: number; y: number} = {x: 12, y: 12};

const printName = (person: {first: string; last: string}) => {
  console.log(`${person.first} ${person.last}`);
};
```

### Type Alias

```typescript
type Person = {
  first_name: string;
  last_name: string;
  nickname?: string; // Optional field
  readonly id: string; // Can only be initialized.
};

const printName = (p: Person) => {};
```

### Intersection Types

```typescript
type Circle = {
  r: number;
};

type Colorful = {
  color: string;
};

type ColorfulCircle = Circle & Colorful;

const r: ColorfulCircle = {
  r: 12,
  color: "red"
};
```

### Array Types

```typescript
const users: User[] = [];
const bools = Array<boolean> = [];      // Using Array generic type
const board = string[][] = [][];        // 2D array
```

### Union Types

```typescript
let age: number | string = 23; // It can both have strings and numbers
age = "34";

if (typeof age === "string") {
}
```

### Literal Types

```typescript
let zero: 0 = 0; // Can only hold 0
let hi: "hi" = "hi";
```

It can have nice results when combines with unions:

```typescript
const giveAnswer = (ans: "yes" | "no") => {};

giveAnswer("maybe"); // Error!
```

### tuple types

Tuples are:

1. array
2. fixed length
3. ordered
4. with specific types

```typescript
type MyType = [number, string];
const x: MyType = [1, "eyval"];
```

### enums

```typescript
enum Response {
  YES,
  NO,
  MAYBE
}

const ans: Response = Response.YES;

enum Response {
  yes = "yes",
  no = "no"
}
```

## Interfaces

Almost the exact same purpose of type aliases. They describe the shape of objects.

```typescript
interface Person {
  first: string;
  last: string;
  age?: number; // optional
  readonly id: number;
  sayHi: () => string; // Anyone wants to be like `Person` should implement a function named `sayHi` that returns a string.
}
```

### Extend interfaces

```typescript
interface Employee extends Person {
  role: string;
  salary: number;
}
```

## Classes

```typescript
class Player {
    readonly name: string;
    age?: number;
    score = 0;

    constructor(first: name, age: number) {
        this.name: first;
        this.age = age;
    }
}
```

Everything in typescript/javascript classes is public by default.

```typescript
class Player {
  private score: number = 0;

  // Class properties shorthands:
  constructor(public firstName: string) {} // firstName will become a public field of the class and can be initialized here as well.
}
```

### Inheritance

```typescript
class X extends Y {}
```

### Classes and Interfaces

```typescript
interface Colorful {
  color: string;
}

class Bike implements Colorful {
  color = "red";
}
```

## Generics

```typescript
const x: Array<number> = [1, 2, 3];
function identity<Type>(t: Type): Type {
  return t;
}

// T and U can only be an object-like type
function merge<T extends object, U extends object>() {}

// Default type
function createArray<T = number>(): T[] {}

// Generic classes:
class Test<Type> {}
```

## Discriminated unions

```typescript
interface Rooster {
    name: string;
    kind: "rooster";
}

interface Cow {
    name: string;
    kind: "cow";
}

interface Dog {
    name: string;
    kind: "dog";
}

type Animal = Dog | Cow | Rooster;

function noise(a: Animal) {
    switch(a.kind) {
        case("dog"):
            ...
        case("rooster"):
            ..
        case("cow"):
            ...
    }
}
```

## Modules

What is module? A file with an `export` or a top level `await` should be considered a **script** not a **module**. Inside a script file, all variables and types are in global scope and can be accessed anywhere.
Otherwise, it needs to be imported before use.

```typescript
//utils.ts

export function add(x: number: y: number): number {
    return x + y
}

// index.ts
import {add} from "./utils"
add(1, 3);
```

### Different types of exports

1. Named export

```typescript
// utils.ts
export const PI = 3.1415;
```

should be imported like this:

```typescript
// index.ts
import {PI} from "./utils";
//or
import {PI as MY_PI} from "./utils";
```

2. Default export

```typescript
// utils.ts
export default function add(x: number, y: number) {}
```

It can be imported with any name like this:

```typescript
import myAdd from "./utils";
```
