// Parametric TPU Drone Front & Rear Mounts (mm)
// Open in OpenSCAD to tweak any dimension and re-export STL if needed.

$fn = 48;

// --- Parameters ---
standoff_height = 25.0;   // Chieu cao lot long giua 2 tam carbon (2.5 cm)
screw_spacing   = 28.0;   // Khoang cach tam 2 oc (2.8 cm)
screw_dia       = 3.2;    // Lo oc M3 (hoac M2 + standoff)
cam_width       = 19.2;   // Be rong than camera Micro (19mm)
rear_hole_dia   = 5.2;    // Lo gan anten duoi (0.5 cm; doi thanh 6.5 neu dung SMA chuan)
ant_tilt_deg    = 35;     // Goc nga anten ra sau

module front_camera_mount() {
    difference() {
        union() {
            // 2 tru bat oc 2 ben
            for (sx = [-1, 1]) {
                translate([sx * screw_spacing / 2, 0, 0])
                    cylinder(h = standoff_height, d = 7.2);
                // Vach kep camera
                translate([sx * (cam_width/2 + 1.4) - 1.4, -2, 0])
                    cube([2.8, 16, standoff_height]);
                // Cau noi tru oc va vach camera
                translate([sx * (screw_spacing/2 - 1.6) - 2.0, -3.5, 0])
                    cube([4.0, 7.0, standoff_height]);
            }
            // De duoi bao ve
            translate([-14.5, -3.5, 0])
                cube([29, 17.5, 2.8]);
            // Thanh ngang tren cung bao ve
            translate([-12, 6.5, standoff_height - 2.8])
                cube([24, 6.5, 2.8]);
        }
        // Lo oc xuyen tam
        for (sx = [-1, 1]) {
            translate([sx * screw_spacing / 2, 0, -1])
                cylinder(h = standoff_height + 2, d = screw_dia);
        }
        // Khe chinh goc camera M2
        translate([-16, 7.5, standoff_height/2])
            rotate([0, 90, 0])
                hull() {
                    translate([-1.5, 0, 0]) cylinder(h = 32, d = 2.1);
                    translate([ 1.5, 0, 0]) cylinder(h = 32, d = 2.1);
                }
    }
}

module rear_antenna_mount() {
    difference() {
        union() {
            for (sx = [-1, 1]) {
                translate([sx * screw_spacing / 2, 0, 0])
                    cylinder(h = standoff_height, d = 7.2);
                // 2 chan duoi kep day/buzzer/ELRS
                translate([sx * 9.5 - 3.2, 5.5, 0])
                    cube([6.4, 12, 5]);
            }
            // Khoi than duoi
            translate([-14.5, -2.5, 0])
                cube([29, 17, standoff_height]);
            // Co nghieng gan anten
            translate([0, 10.5, 18])
                rotate([-ant_tilt_deg, 0, 0])
                    cylinder(h = 16, d = 14.4, center = true);
        }
        // Lo oc xuyen tam
        for (sx = [-1, 1]) {
            translate([sx * screw_spacing / 2, 0, -1])
                cylinder(h = standoff_height + 2, d = screw_dia);
        }
        // Lo anten + 2 lo day rut
        translate([0, 10.5, 18])
            rotate([-ant_tilt_deg, 0, 0]) {
                cylinder(h = 40, d = rear_hole_dia, center = true);
                for (sx = [-1, 1])
                    translate([sx * 8.5, 0, 0])
                        cylinder(h = 40, d = 2.6, center = true);
            }
        // Khe luon day o giua phia duoi
        translate([-3.5, 4, -1])
            cube([7, 16, 15]);
    }
}

// Bo comment 1 trong 2 dong duoi de render tung mon:
front_camera_mount();
translate([45, 0, 0]) rear_antenna_mount();
