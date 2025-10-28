set of int: arr1 = 0..5;
set of int: arr2 = 0..5;

constraint forall(x in arr1)(x >= 0);
constraint exists(x in arr2)(x > 3);