d=15.6;
l=20;
cone=2;

// base
bd=d+2;
bl=1.5;

// shaft
sd=5.3;
sw=3.2;
sl=7;

difference()
{
	intersection()
	{
		union()
		{
			translate([0, 0, -bl]) cylinder(h=bl, d=bd);
			translate([0, 0, 0]) cylinder(h=l-cone, d1=d, d2=d-0.6, $fn=50);
			translate([0, 0, l-cone]) cylinder(h=cone, d1=d-0.6, d2=d-0.6-cone/2, $fn=50);

			
			rotate([0, 0,  0]) translate([0, 0, l/2]) cube([1, d+1.2, l], center=true);
			rotate([0, 0, 60]) translate([0, 0, l/2]) cube([1, d+1.2, l], center=true);
			rotate([0, 0,-60]) translate([0, 0, l/2]) cube([1, d+1.2, l], center=true);
		}
		translate([0, 0, -bl-0.1]) cylinder(h=l+bl+0.2, d1=l*2, d2=d-0.6-cone/2, $fn=50);
	}
	intersection()
	{
		translate([0, 0, -bl-0.1]) cylinder(h=sl, d=sd, $fn=40);
		translate([0, 0, sl/2-bl-0.1]) cube([sw, sd, sl], center=true);
	}
}