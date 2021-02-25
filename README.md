# picturedsk
`picturedsk` generates a custom Apple II 5.25" disk image in the WOZ format that "imprints" an arbitrary input image into the magnetic flux of the disk. It also incorporates onto the disk a small bootable screen image and optional message. You can try this program and boot the output image using any WOZ-compatible emulator, but for most impressive results, you should have the [Applesauce](https://applesaucefdc.com) application and hardware to visualize and write a physical floppy.

![Some palm trees in life and on floppy](http://decaf.co/picturedsk_palm.png)

### Requires

- An input image, in BMP (Windows Bitmap) format. Any dimensions are OK, but if it's not square, the result will get squished into a square. The input can be colored, but bear in mind that the output is only 1-bit, so something low-detail and high-contrast will look best.
- The output WOZ file will boot on various contemporary emulators (like [Virtual II](http://www.virtualii.com)), but is really designed to be written onto physical media with a full Applesauce setup (controller and drive with sync sensor). The resulting disk will boot on a real machine and will also have an extremely cool flux image.
- You need some basic C compiler setup to build the program (see below). 

### How to build, run, and see the results

1. There are no dependencies besides the standard C libraries. The included `Makefile` will build the source files into a command line utility called `picturedsk`:

    `make`
    
2. Run the program by giving it an input image file (supports BMP format only) and an output file name. There is an optional message string, and if you supply one as the final argument, it will appear on the screen when the disk image boots:

    `./picturedsk my_image.bmp output.woz "HELLO FLOPPY"`
    
3. Take the resulting .WOZ file, and open it in the Applesauce application's _Disk Writer_ mode. Write it to a fresh 5.25" floppy (make sure "Force Track Synchronization" is checked).

4. Try booting the disk you just made. Then use Applesauce's _Flux Imager_ to image the disk and see what your image looks like as concentric flux circles.

### What is this actually useful for?

This is really more art project than useful product. Maybe create custom holiday gifts for your retro-computing friends? 

![Palms on the screen](http://decaf.co/picturedsk_screen.png)

### Thanks

The Apple GCR encoding algorithm was cribbed from [dsk2woz](https://github.com/TomHarte/dsk2woz) by Tom Harte. Some inspiration comes from Antoine Vignau's [graffidisk](http://www.brutaldeluxe.fr/products/apple2/graffidisk/). Thanks, of course, to John K. Morris, the prime mover behind the Applesauce project and a perennially helpful fellow. 

## How does it work?

The WOZ image and Applesauce allows for the representation of arbitrary nibble streams on arbitrary quarter-tracks. This program produces a custom WOZ disk image filled mostly with a "texture mapped" sampling of the input image. The disk image also contains a single valid, bootable track (outer track 0) which displays a version of that same image on-screen if you boot it. 

Track 0 has a valid boot sector, a small image loading program, and an encoding of the input image in the Apple HGR format. This fills up almost the whole track. The boot sector loads the track starting at $B000, and then jumps there. That next bit of code copies the image data (which starts at $B100) to the interleaved HGR memory for display, prints a text message, and then infinite-loops. The bitmap displayed is smaller than full-screen, using the constrained dimensions to allow its data to fit entirely within track 0. 

The rest of the tracks are created by using a polar coordinate system to sample the same input image at every nibble location around the track. The sampling both here and for the HGR version as above are done using greyscale luma threshold to transform 24-bit RGB to 1-bit monochrome. These tracks are specified to be written at every third quarter-track (rather than every fourth) to get a higher "resolution" in the flux image; since they're not readable anyway there is no data safety concern. 
