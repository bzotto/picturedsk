# picturedsk
`picturedsk` generates a custom Apple II 5.25" disk image in the WOZ format that "imprints" an arbitrary input image into the magnetic flux of the disk. It also incorporates onto the disk a small bootable screen image and optional message. For most impressive results, you should have the [Applesauce](https://applesaucefdc.com) application and hardware. 

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
