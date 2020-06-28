// base plate
bw=16;
bl=60;
bt=1.6;

// mount
mw=16;
ml=36;
mh=3.5;
mt=1.6;
ma=2.5; // arms

//pads
pw=5.5;
pl=3;
ph=0.2;
po=12;

// holes
hd=4;
hl=5;
ho=24.5;

module hole(offset, diam)
{
  hull()
  {
    translate([-hl/2, offset, -1]) cylinder(h=bt*2+2, d=diam, center=true, $fn=30);
    translate([ hl/2, offset, -1]) cylinder(h=bt*2+2, d=diam, center=true, $fn=30);
  }
}

//rotate([0,90,0])
{
    difference()
    {
      union()
      {
        translate([-bw/2, -bl/2, -bt]) cube([bw, bl, bt]);

        translate([0, po,0]) cube([pw, pl, ph*2], center=true);
        translate([0,-po,0]) cube([pw, pl, ph*2], center=true);
      }
      hole(-ho, hd);
      hole(ho, hd);
    }

    translate([-bw/2, -ml/2-mt, -bt]) cube([mw, mt, bt+mh+mt/2]);
    translate([-bw/2,  ml/2, -bt]) cube([mw, mt, bt+mh+mt/2]);

    hull()
    {
        translate([0, -ml/2+ma-mt/2, mh+mt/2]) rotate([0, 90, 0]) cylinder(h=mw, d=mt, center=true, $fn=10);
        translate([0, -ml/2-mt/2, mh+mt/2]) rotate([0, 90, 0]) cylinder(h=mw, d=mt, center=true, $fn=10);
    }

    hull()
    {
        translate([0, ml/2-ma+mt/2, mh+mt/2]) rotate([0, 90, 0]) cylinder(h=mw, d=mt, center=true, $fn=10);
        translate([0, ml/2+mt/2, mh+mt/2]) rotate([0, 90, 0]) cylinder(h=mw, d=mt, center=true, $fn=10);
    }
}