d=15.6;
l=25;

// base
bd=d+2;
bl=1.5;

// shaft
sd=5.6;
sw=3.5;
sl=8;

difference()
{
	union()
	{
		translate([0, 0, -bl]) cylinder(h=bl, d=bd);
		translate([0, 0, 0]) cylinder(h=l, d1=d, d2=d-0.6, $fn=50);

		rotate([0, 0,  0]) translate([0, 0, l/2]) cube([1, d+1.2, l], center=true);
		rotate([0, 0, 60]) translate([0, 0, l/2]) cube([1, d+1.2, l], center=true);
		rotate([0, 0,-60]) translate([0, 0, l/2]) cube([1, d+1.2, l], center=true);
	}
	intersection()
	{
		translate([0, 0, -bl-0.1]) cylinder(h=sl, d=sd, $fn=40);
		translate([0, 0, sl/2-bl-0.1]) cube([sw, sd, sl], center=true);
	}
}