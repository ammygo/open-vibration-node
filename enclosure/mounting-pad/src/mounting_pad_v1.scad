// Mounting Pad for Vibration Sensor with physical M6 Thread
$fn = 64;

top_plate_diameter = 30.0;
top_plate_thickness = 3.0;

hex_across_flats = 13.0;
hex_thickness = 6.0;

stud_diameter = 6.0; 
stud_length = 9.0;

module hexagon(flat_to_flat, height) {
    r = (flat_to_flat / 2.0) / cos(30);
    cylinder(r = r, h = height, $fn = 6);
}

// Helical thread module for OpenSCAD (useful for 3D printing)
module helical_thread(dia, pitch, length) {
    r_major = dia / 2.0;
    r_minor = r_major - 0.613 * pitch; // standard metric thread depth
    turns = length / pitch;
    steps_per_turn = 32;
    total_steps = turns * steps_per_turn;
    
    // We construct the thread by stacking small slices or cylinders
    // For a simple visual representation in OpenSCAD, we stack thin layers:
    for (i = [0 : total_steps - 1]) {
        z1 = -length + (i / total_steps) * length;
        z2 = -length + ((i + 1) / total_steps) * length;
        
        // Vary radius to simulate a thread profile
        angle = 360.0 * i / steps_per_turn;
        // Simple triangular thread approximation
        factor = 0.5 + 0.5 * sin(angle);
        r1 = r_minor + (r_major - r_minor) * factor;
        
        angle2 = 360.0 * (i + 1) / steps_per_turn;
        factor2 = 0.5 + 0.5 * sin(angle2);
        r2 = r_minor + (r_major - r_minor) * factor2;
        
        translate([0, 0, z1])
            cylinder(r1 = r1, r2 = r2, h = (z2 - z1), $fn = 24);
    }
}

union() {
    // 1. Physical M6 Threaded Stud (Pitch = 1.0mm)
    helical_thread(stud_diameter, 1.0, stud_length);
    
    // 2. Hexagon section
    hexagon(hex_across_flats, hex_thickness);
        
    // 3. Top flat plate
    translate([0, 0, hex_thickness])
        cylinder(d = top_plate_diameter, h = top_plate_thickness);
}
