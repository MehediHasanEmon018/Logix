    s = 0
    i = 0
L1:
    t1 = i < 10
    ifFalse t1 goto L2
    t2 = i == 2
    ifFalse t2 goto L4
    goto L3
L4:
    t3 = i == 6
    ifFalse t3 goto L5
    goto L2
L5:
    t4 = s + i
    s = t4
L3:
    t5 = i + 1
    i = t5
    goto L1
L2:
    j = 0
L6:
    t6 = j + 1
    j = t6
    t7 = j < 3
    ifFalse t7 goto L7
    goto L6
L7:
    print s
    print j
