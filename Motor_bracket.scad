base_h=35.5;
base_w=17;
base_t=3.0;
base_space=base_h+7; //42;

fix_h=4.5;
fix_w=7;
fix_d=24/2;

arm_t=1.5;
arm_l=46.5;
arm_w=10;
arm_dist=35/2;
arm_hole=4;
arm_hole_l1=15;
arm_hole_l2=30;

side_t=1.6;
full_h=59;
full_w=21.5;
back_r=5;
front_r=7;
sw_hole=3;
sw_t=1.6;
b_o=1;

motor_hole_w=10;
motor_hole_l=30;
motor_shift=2.2;

//micro switch
msw_w=6+0.6;
msw_h=7;
msw_l=13;
msw_t=1.6;
msw_conn=3;

// cap
cap_t=1.0;
cap_sp=0.4; // space
cap_fix_h=5;
cap_fix_t=2;
cap_fix_w=5;
cap_fix_ofs=4.5;
lock_h=2;
lock_w=30;

module arm()
{
	hull()
	{
		translate([0, arm_hole_l1, -arm_t-1]) cylinder(h=arm_t+2, d=arm_hole, $fn=20);
		translate([0, arm_hole_l2, -arm_t-1]) cylinder(h=arm_t+2, d=arm_hole, $fn=20);
	}
}

module sides(hh, r_b, r_f, b, shrink)
{
	hull()
	{
	translate([ full_h/2-r_b, r_b-side_ofs, -b]) cylinder(h=hh+arm_t, r=r_b-shrink);
	translate([-full_h/2+r_b, r_b-side_ofs, -b]) cylinder(h=hh+arm_t, r=r_b-shrink);
	translate([ full_h/2-r_f, arm_l-r_f, -b]) cylinder(h=hh+arm_t, r=r_f-shrink);
	translate([-full_h/2+r_f, arm_l-r_f, -b]) cylinder(h=hh+arm_t, r=r_f-shrink);
	}
}

side_ofs=base_t-b_o;

module base()
{
	translate([motor_shift,0,0]) difference()
	{
		translate([-base_h/2, -base_t, -arm_t]) cube([base_h, base_t, base_w+arm_t]);
		translate([-fix_d-fix_h/2, -base_t-1, arm_w/2]) cube([fix_h, 2, fix_w]);
		translate([ fix_d-fix_h/2, -base_t-1, arm_w/2]) cube([fix_h, 2, fix_w]);
	}

	difference()
	{
		sides(full_w, back_r, front_r, arm_t, 0, $fn=70);
		sides(full_w+2, back_r, front_r, 0, side_t, $fn=40);
		translate([-base_space/2+motor_shift, -side_t-1, 0]) cube([base_space+sw_hole, side_t+2, full_w+1]);
		mirror([1,0,0])
		translate([-arm_dist-motor_shift, 0, 0]) arm();
		translate([-arm_dist+motor_shift, 0, 0]) arm();
		//translate([-motor_hole_w/2+motor_shift, 0, -base_t-1]) cube([motor_hole_w, arm_l-side_t, base_t+2]);
		hull()
		{
			translate([motor_shift, motor_hole_w/2, -base_t-1]) cylinder(d=motor_hole_w, h=base_t+2, $fn=30);
			translate([motor_shift, -motor_hole_w/2+motor_hole_l, -base_t-1]) cylinder(d=motor_hole_w, h=base_t+2, $fn=30);
		}
		
		
		translate([full_h/2-sw_t-sw_hole, back_r-side_ofs, -arm_t-1]) cube([sw_hole, arm_l+side_ofs-back_r-front_r, arm_t+full_w+2]);
		translate([full_h/2-sw_t-sw_hole/2, arm_l-front_r, -arm_t-1]) cylinder(h=arm_t+full_w+2, r=sw_hole/2, $fn=10);
		intersection()
		{
			translate([ full_h/2-back_r, back_r-side_ofs, -arm_t-1]) cylinder(h=full_w+arm_t+2, r=back_r-sw_t, $fn=30);
			translate([ full_h/2-sw_t-sw_hole, back_r-side_ofs-10+0.01, -arm_t-1]) cube([10,10,full_w+arm_t+2]);
		}
		translate([+base_space/2+motor_shift, -side_t-1, -arm_t-1]) cube([sw_hole, side_t+2, full_w+1]);

	}

	translate([full_h/2-sw_t-sw_hole-msw_h-msw_t+msw_h+msw_t-side_t, back_r-side_ofs+msw_w+msw_t*2-side_t, 0]) 
	cube([side_t, arm_l-(back_r-side_ofs)-(msw_w+msw_t*2), full_w]);
	// microswitch
	translate([full_h/2-sw_t-sw_hole-msw_h-msw_t, back_r-side_ofs, 0]) 
	difference()
	{
		cube([msw_h+msw_t, msw_w+msw_t*2, msw_l]);
		translate([msw_t, msw_t, 0]) cube([msw_h+1, msw_w, msw_l+1]);
		translate([-1, msw_t+msw_w/2-msw_conn/2, 0]) cube([msw_t+2, msw_conn, msw_l+1]);
	}

	// PCB holder
	translate([-full_h/2+side_t+2+1.5+1.5+2.5, 35, 0]) cube([5, 3.5, 12]);
	translate([-full_h/2+side_t+2+1.5, 35, 0]) cube([5+1.5+2.5, 3.5, 2]);
	translate([-full_h/2+side_t+2+1.5, 12, 0]) cube([1.8, 30, 2]);

	%translate([-full_h/2+side_t+2, 0, 0])
	{
		color("green") cube([1.5, 43.2, 21]);
		color("blue") translate([1.5,21.9,3])cube([1.5, 21, 16]);
		color("grey") translate([3,23.5,5])cube([2.5, 15, 12]);
	}

	// lock
	hull()
	{
		translate([-lock_w/2, arm_l-side_t-lock_h/2, full_w-0.1]) cube([lock_w, side_t, 0.1]);
		translate([-lock_w/2, arm_l-side_t, full_w-lock_h]) cube([lock_w, side_t, 0.1]);
	}
}

module cap()
{
  difference()
	{
		hull()
		{
			$fn=50;
			translate([ full_h/2-back_r, back_r-side_ofs, 0]) cylinder(h=cap_t, r=back_r);
			translate([-full_h/2+back_r, back_r-side_ofs, 0]) cylinder(h=cap_t, r=back_r);
			translate([ full_h/2-front_r, arm_l-front_r, 0]) cylinder(h=cap_t, r=front_r);
			translate([-full_h/2+front_r, arm_l-front_r, 0]) cylinder(h=cap_t, r=front_r);
		}
		translate([full_h/2-sw_t-sw_hole, back_r-side_ofs-100, -cap_t+cap_sp]) cube([sw_hole+100, arm_l+side_ofs-back_r-front_r+100, cap_t]);
		translate([+base_space/2+motor_shift, -side_t-1, -cap_t+cap_sp]) cube([sw_hole, side_t+2, cap_t]);
	}
	translate([motor_shift-base_space/2+cap_sp, -base_t, -full_w+base_w]) cube([base_space-cap_sp, base_t+cap_sp, full_w-base_w+cap_t]);
	translate([motor_shift-base_h/2+cap_fix_ofs, cap_sp, -(full_w-base_w+cap_t+cap_fix_h)]) cube([cap_fix_w, 3, full_w-base_w+cap_t+cap_fix_h]);
	translate([motor_shift+base_h/2-cap_fix_ofs-cap_fix_w, cap_sp, -(full_w-base_w+cap_t+cap_fix_h)]) cube([cap_fix_w, 3, full_w-base_w+cap_t+cap_fix_h]);
	translate([-full_h/2+side_t+2+1.5+1.5+2.5, 35, -cap_fix_h-2]) cube([5, 3.5, cap_fix_h+2]);
	translate([-full_h/2+side_t+2+1.5+1.5+2.5, 35, -cap_fix_h]) cube([5, arm_l-35-side_t-cap_sp, cap_fix_h]);

	// front lock
	hull()
	{
		translate([-full_h/2+side_t+2+1.5+1.5+2.5, arm_l-side_t-2-cap_sp-lock_h/2,  0]) cube([full_h-front_r-side_t-(2+1.5+1.5+2.5), 2, 0.1]);
		translate([-full_h/2+side_t+2+1.5+1.5+2.5, arm_l-side_t-2-cap_sp, -lock_h]) cube([full_h-front_r-side_t-(2+1.5+1.5+2.5), 2, 0.1]);
	}

	translate([full_h/2-sw_t-sw_hole-side_t-5, 35-5+arm_l-35-side_t-cap_sp, -cap_fix_h]) cube([5, 5, cap_fix_h]);
	// mswitch
//	translate([full_h/2-sw_t-sw_hole-msw_h-msw_t, back_r-side_ofs, -(full_w-msw_l)]) 
//		cube([msw_h+msw_t, msw_w+msw_t*2, full_w-msw_l]);
}

//base(); //translate([0, 0, full_w+0.2]) cap();
rotate([0,180,0]) cap();