int: value;

constraint -value < 150;
constraint 150 > -value;

output ["negative value is \(-value)\n"];