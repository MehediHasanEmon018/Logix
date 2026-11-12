int a = 5;
int b = a * 2 + 1;
float f = 3;
bool ok = a < b && !false;
{
    int a = a + 1;    // initializer reads the OUTER a
    print a;
}
print f;
print ok;
