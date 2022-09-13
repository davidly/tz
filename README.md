# tz
TifZip. Windows command-line app that sets the compression state on a tiff file to Zip, LZW, or uncompressed.

Use m.bat in a Visual Studio 64 bit build window to build.

    usage: tz imagepath [/i] [/m:X]
    
       sets tiff compression to ZIP, LZW, or uncompressed.
      
    arguments:
    
        [/i]     information only; don't compress the image
        [/m:X]   method of compression. X is Z=Zip, L=LZW, U=Uncompressed. Default is Z
