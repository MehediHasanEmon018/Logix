# Formal CFG (matches src/parser/parser.y)

```
program   -> stmt_list
stmt_list -> ε | stmt_list stmt
stmt      -> decl ';'
           | lvalue ('=' | '+=' | '-=' | '*=' | '/=' | '%=') expr ';'
           | lvalue ('++' | '--') ';'   |  ('++' | '--') lvalue ';'
           | 'print' expr ';'
           | 'printf' '(' STRING [',' expr_list] ')' ';'
           | 'scanf'  '(' STRING ',' '&' lvalue {',' '&' lvalue} ')' ';'
           | ID '(' [expr_list] ')' ';'                        (function call)
           | 'if' '(' expr ')' stmt
           | 'if' '(' expr ')' stmt 'else' stmt                (else-if = else + if)
           | 'while' '(' expr ')' stmt
           | 'do' stmt 'while' '(' expr ')' ';'
           | 'for' '(' [decl | simple] ';' [expr] ';' [simple] ')' stmt
           | 'break' ';'  |  'continue' ';'
           | 'return' [expr] ';'
           | func_def
           | block
block     -> '{' stmt_list '}'
decl      -> type declarator {',' declarator}
declarator-> ID | ID '=' expr
           | ID '[' INT_LIT ']'  |  ID '[' INT_LIT ']' '=' '{' expr_list '}'  |  ID '[' ']' '=' '{' expr_list '}'
func_def  -> (type | 'void') ID '(' [params] ')' (block | ';')   (';' = prototype; 'main' is compiled inline)
params    -> 'void' | param {',' param}
param     -> type ID | type ID '[' ']'
lvalue    -> ID | ID '[' expr ']'
type      -> 'int' | 'float' | 'bool'        ('char' = int, 'double' = float)
expr      -> expr '||' expr | expr '&&' expr
           | expr ('==' | '!=') expr
           | expr ('<' | '>' | '<=' | '>=') expr
           | expr ('+' | '-') expr
           | expr ('*' | '/' | '%') expr
           | '!' expr | '-' expr | '(' expr ')'
           | lvalue | ID '(' [expr_list] ')' | INT_LIT | FLOAT_LIT | 'true' | 'false'
```

Precedence (low to high): `||`, `&&`, `== !=`, `< > <= >=`, `+ -`, `* / %`, `!`. All binary operators are left-associative.
Dangling else is resolved by `%prec LOWER_THAN_ELSE` (else binds to the nearest if).
