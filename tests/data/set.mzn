set of int: a = {1, 3, 6, 7, 8};
set of int: b = {3, 7, 8};
set of int: c = {1, 6};

constraint a superset b;
constraint b union c == a;
