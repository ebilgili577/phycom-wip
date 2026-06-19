$fn = 36;
button_r = 3.35/2;

hole_r = 10/2;
h = 4;
little_h = 3;

button_travel = 0.3;

button_extra = 0.1;

hole_extra = 3/2;


// white one, good
module button_cover() {
    difference() {
    
        // hole
        cylinder(h=h+button_travel, r=hole_r+hole_extra);
        // button
        #cylinder(h=little_h-0.2, r=button_r+button_extra);
    }

}

module main() {

    button_cover();
    
}

main();
