# Formal CFG (matches src/parser/parser.y)

```
program   -> stmt_list
stmt_list -> ε | stmt_list stmt
stmt      -> decl ';'
           | ID '=' expr ';'
           | 'print' expr ';'
           | 'if' '(' expr ')' stmt
           | 'if' '(' expr ')' stmt 'else' stmt
           | 'while' '(' expr ')' stmt
           | block
block     -> '{' stmt_list '}'
decl      -> type ID | type ID '=' expr        (declaration with initializer)
type      -> 'int' | 'float' | 'bool'
expr      -> expr '||' expr | expr '&&' expr
           | expr ('==' | '!=') expr
           | expr ('<' | '>' | '<=' | '>=') expr
           | expr ('+' | '-') expr
           | expr ('*' | '/' | '%') expr
           | '!' expr | '(' expr ')'
           | ID | INT_LIT | FLOAT_LIT | 'true' | 'false'
```

Precedence (low to high): `||`, `&&`, `== !=`, `< > <= >=`, `+ -`, `* / %`, `!`. All binary operators are left-associative.
Dangling else is resolved by `%prec LOWER_THAN_ELSE` (else binds to the nearest if).

## C extensions (also in src/parser/parser.y)

```
stmt      -> ... | lvalue ('=' | '+=' | '-=' | '*=' | '/=' | '%=') expr ';'
           | lvalue ('++' | '--') ';' | ('++' | '--') lvalue ';'
           | call ';'
           | 'printf' '(' STRING_LIT (',' expr)* ')' ';'
           | 'scanf' '(' STRING_LIT (',' '&' ID ('[' expr ']')?)+ ')' ';'
           | 'return' expr? ';' | 'break' ';' | 'continue' ';'
           | 'for' '(' for_init ';' expr? ';' simple? ')' stmt
           | 'do' stmt 'while' '(' expr ')' ';'
           | 'switch' '(' expr ')' '{' ('case' expr ':' stmt_list | 'default' ':' stmt_list)* '}'
           | func_def
func_def  -> (type | 'void') ID '(' params? ')' (block | ';')        (';' = prototype)
param     -> type ID | type ID '[' ']' | type ID '[' INT_LIT ']'
decl      -> type ID '[' expr ']' ('=' '{' expr_list '}')?
           | type ID '[' ']' '=' '{' expr_list '}'
lvalue    -> ID | ID '[' expr ']'
expr      -> ... | expr '?' expr ':' expr | '(' type ')' expr
           | ID '(' expr_list? ')' | ID '[' expr ']'
```

Lexer extras: `char` = int, `double` / `long double` = float, `long`, `long long`, `short` = int,
`const` / `unsigned` / `signed` / `static` are ignored, `#define NAME number|'c'` works as a constant,
other `#` lines are ignored. Built-ins: `sqrt pow fabs floor ceil` (float) and `abs` (int).

Semantic rules kept from Logix: conditions of if/while/for/do-while/`?:` must be `bool`
(write `if (n % 2 == 0)`, not `if (n % 2)`); `&&` `||` `!` need bool operands;
float -> int needs an explicit `(int)` cast.

TAC additions: `array a, n, int`, `t = a[i]`, `a[i] = t`, `x = read int|float|char`,
`print x, "%d"`, `print "text"`, `param x`, `t = call f, n`, `func f, p1, p2` ... `endfunc`, `return x`.
