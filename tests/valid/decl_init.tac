    a = 5
    t1 = a * 2
    t2 = t1 + 1
    b = t2
    t3 = (float) 3
    f = t3
    t4 = a < b
    t5 = t4
    ifFalse t5 goto L1
    t6 = ! false
    t5 = t6
L1:
    ok = t5
    t7 = a + 1
    a_1 = t7
    print a_1
    print f
    print ok
