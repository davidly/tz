//
// Gets and/or sets the compression state of a tiff file to uncompressed, zip, or lzw.
// Updates are done by creating a whole new file and renaming that to the original filename.
//

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <wincodecsdk.h>
#include <stdio.h>
#include <wrl.h>
#include <comdef.h>

#include <djl_tz.hxx>

using namespace Microsoft::WRL;

#pragma comment( lib, "ole32.lib" )
#pragma comment( lib, "oleaut32.lib" )
#pragma comment( lib, "windowscodecs.lib" )
#pragma comment( lib, "ntdll.lib" )

CDJLTrace tracer;

static void Usage()
{
    printf( "usage: tz imagepath [/i] [/m:X] \n" );
    printf( "  sets tiff compression to ZIP, LZW, or uncompressed.\n" );
    printf( "arguments:\n" );
    printf( "  [/i]     information only; don't modify the image\n" );
    printf( "  [/m:X]   method of compression. X is Z=Zip, L=LZW, U=Uncompressed. Default is Z\n" );
    printf( "  [/t]     enable debug tracing to tz.txt. Use -T to empty tz.txt first\n" );
    exit( 1 );
} //Usage

DWORD GetSize( WCHAR const * pwc )
{
    DWORD size = 0;

    HANDLE h = CreateFile( pwc, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0 );

    if ( INVALID_HANDLE_VALUE != h )
    {
        size = GetFileSize( h, 0 );
        CloseHandle( h );
    }

    return size;
} //GetSize

const char * CompressionType( DWORD x )
{
    if ( 1 == x )
        return "uncompressed";
    if ( 6 == x )
        return "JPEG (old-style)";
    if ( 7 == x )
        return "JPEG";
    if ( 8 == x )
        return "deflate (zip)";
    if ( 5 == x )
        return "LZW";
    if ( 99 == x )
        return "JPEG";
    if ( 32773 == x )
        return "PackBits";

    return "unknown";
} //CompressionType

void PrintNumberWithCommas( long long n )
{
    if ( n < 0 )
    {
        printf( "-" );
        PrintNumberWithCommas( -n );
        return;
    }
   
    if ( n < 1000 )
    {
        printf( "%lld", n );
        return;
    }

    PrintNumberWithCommas( n / 1000 );
    printf( ",%03lld", n % 1000 );
} //PrintNumberWithCommas

void ShowErrorAndExit( char * pcMessage, HRESULT hr = 0 )
{
    if ( S_OK == hr )
    {
        printf( "%s\n", pcMessage );
    }
    else
    {
        _com_error err( hr );
        printf( "%s; error %#x == %ws\n", pcMessage, hr, err.ErrorMessage() );
    }

    exit( 1 );
} //ShowErrorAndExit

extern "C" int __cdecl wmain( int argc, WCHAR * argv[] )
{
    WCHAR * pwcPath = NULL;
    BOOL infoOnly = false;
    DWORD compressionMethod = 8;
    bool enableTracer = false;
    bool emptyTracerFile = false;

    for ( int iArg = 1; iArg < argc; iArg++ )
    {
        const WCHAR * pwcArg = argv[iArg];
        WCHAR a0 = pwcArg[0];

        if ( ( L'-' == a0 ) ||
             ( L'/' == a0 ) )
        {
           WCHAR a1 = towlower( pwcArg[1] );

           if ( L'i' == a1 )
               infoOnly = true;
           else if ( L'm' == a1 )
           {
               if ( ':' != pwcArg[2] )
                   Usage();

               WCHAR m = tolower( pwcArg[3] );

               if ( 'u' == m )
                   compressionMethod = 1;
               else if ( 'l' == m )
                   compressionMethod = 5;
               else if ( 'z' == m )
                   compressionMethod = 8;
               else
                   Usage();
           }
           else if ( L't' == a1 )
           {
               enableTracer = true;
               if ( 'T' == pwcArg[1] )
                   emptyTracerFile = true;
           }
           else
               Usage();
        }
        else
        {
            if ( 0 != pwcPath )
                Usage();

            pwcPath = argv[ iArg ];
        }
    }

    if ( NULL == pwcPath )
        Usage();

    printf( "input file: %ws\n", pwcPath );

    tracer.Enable( enableTracer, L"tz.txt", emptyTracerFile );

    static WCHAR awcInputPath[ MAX_PATH ];
    WCHAR * pwcResult = _wfullpath( awcInputPath, pwcPath, _countof( awcInputPath ) );
    if ( NULL == pwcResult )
        ShowErrorAndExit( "call to _wfullpath failed" );

    DWORD attributes = GetFileAttributes( pwcResult );
    if ( INVALID_FILE_ATTRIBUTES == attributes )
        ShowErrorAndExit( "the input file doesn't exist" );

    if ( attributes & FILE_ATTRIBUTE_READONLY )
        ShowErrorAndExit( "the input file is read-only" );

    if ( attributes & FILE_ATTRIBUTE_DIRECTORY )
        ShowErrorAndExit( "the input file is a directory" );

    HRESULT hr = CoInitializeEx( NULL, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE );
    if ( FAILED( hr ) )
        ShowErrorAndExit( "can't coinitializeex", hr );

    ComPtr<IWICImagingFactory> wicFactory;
    hr = CoCreateInstance( CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, __uuidof( IWICImagingFactory ),
                           reinterpret_cast<void **>  ( wicFactory.GetAddressOf() ) );
    if ( FAILED( hr ) )
        ShowErrorAndExit( "can't initialize wic", hr );

    CTiffCompression tiffCompression;
    DWORD currentCompression = 0;
    hr = tiffCompression.GetTiffCompression( wicFactory, awcInputPath, &currentCompression );
    if ( FAILED( hr ) )
        ShowErrorAndExit( "unable to read the current compression status", hr );

    printf( "the current compression is %d == %s for file %ws\n", currentCompression, CompressionType( currentCompression ), awcInputPath );

    if ( infoOnly || compressionMethod == currentCompression )
        exit( 0 );

    DWORD sizeBefore = GetSize( awcInputPath );

    hr = tiffCompression.CompressTiff( wicFactory, awcInputPath, compressionMethod );
    if ( FAILED( hr ) )
        ShowErrorAndExit( "unable to set compression", hr );

    DWORD sizeAfter = GetSize( awcInputPath );

    printf( "original file size " );
    PrintNumberWithCommas( sizeBefore );
    printf( " new size " );
    PrintNumberWithCommas( sizeAfter );
    printf( "\n" );
    
    wicFactory.Reset();
    CoUninitialize();

    return 0;
} //wmain

