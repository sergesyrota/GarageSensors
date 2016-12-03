platform_h=4;
screw_d=5.5;
screw_head_h=4;
screw_head_d=8.5;
leg_hole_d=4.5;
leg_spacer=3;
mount_distance=[58.6, 33.3];
platform=[
    mount_distance[0]+leg_hole_d*1.5,
    mount_distance[1]+leg_hole_d*1.5,
    4
    ];

difference() {
    // Platform
    cube(platform);
    //screw holes
    for (i=[-1,2,1]) {
        translate([platform[0]/2+i*platform[0]/3, platform[1]/2, 0]) screw();
    }
}
// mount leg
for (i=[0,1,1]) { 
    for (j=[0,1,1]) {
        translate([i*mount_distance[0]+leg_hole_d*1.5/2,j*mount_distance[1]+leg_hole_d*1.5/2,platform_h]) leg();
    }
}


module screw() {
    cylinder(d=screw_d, h=platform[2]);
    translate([0,0,platform[2]-screw_head_h]) cylinder(d1=screw_d, d2=screw_head_d, h=screw_head_h);
}

module leg() {
    board_h=2;
    lock_h=1;
    difference() {
        union() {
            cylinder(d=leg_hole_d, h=leg_spacer+board_h+lock_h, $fn=15);
            cylinder(d=leg_hole_d*1.5, h=leg_spacer);
            // mount lock skirt
            translate([0,0,leg_spacer+board_h]) scale([1,0.85,1]) cylinder(d1=leg_hole_d+1, d2=leg_hole_d, h=lock_h, $fn=15);
        }
        // screw cutout
        cylinder(d=1.5, h=leg_spacer+board_h+lock_h, $fn=10);
        #translate([-0.5,-leg_hole_d,leg_spacer]) cube([1, leg_hole_d*2, board_h+lock_h]);
    }
}