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

using namespace Microsoft::WRL;

#pragma comment( lib, "ole32.lib" )
#pragma comment( lib, "oleaut32.lib" )
#pragma comment( lib, "windowscodecs.lib" )
#pragma comment( lib, "ntdll.lib" )

ComPtr<IWICImagingFactory> g_IWICFactory;

static void Usage()
{
    printf( "usage: tz imagepath [/i] [/m:X] \n" );
    printf( "  sets tiff compression to ZIP, LZW, or uncompressed.\n" );
    printf( "arguments:\n" );
    printf( "  [/i]     information only; don't modify the image\n" );
    printf( "  [/m:X]   method of compression. X is Z=Zip, L=LZW, U=Uncompressed. Default is Z\n" );
    exit( 1 );
} //Usage

void CreateOutputPath( WCHAR const * pwcIn, WCHAR * pwcOut )
{
    WCHAR const * dot = wcsrchr( pwcIn, L'.' );

    if ( !dot )
    {
        printf( "can't find the file extension in the input filename\n" );
        Usage();
    }

    wcscpy( pwcOut, pwcIn );
    wcscpy( pwcOut + ( dot - pwcIn ), L"-temp" );
    wcscat( pwcOut, dot );
} //CreateOutputPath

HRESULT SetTiffCompression( WCHAR const * pwcPath, WCHAR const * pwcOutputPath, DWORD compressionMethod )
{
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = g_IWICFactory->CreateDecoderFromFilename( pwcPath, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf() );
    if ( FAILED( hr ) )
    {
        printf( "hr from create decoder: %#x\n", hr );
        return hr;
    }

    GUID containerFormat;
    hr = decoder->GetContainerFormat( &containerFormat );
    if ( FAILED( hr ) )
    {
        printf( "hr from GetContainerFormat: %#x\n", hr );
        return hr;
    }

    if ( GUID_ContainerFormatTiff != containerFormat )
    {
        printf( "container format of the input file isn't TIFF, so not compressing\n" );
        return E_FAIL;
    }

    ComPtr<IWICStream> fileStream = NULL;
    hr = g_IWICFactory->CreateStream( fileStream.GetAddressOf() );
    if ( FAILED( hr ) )
    {
        printf( "create stream hr: %#x\n", hr );
        return hr;
    }

    hr = fileStream->InitializeFromFilename( pwcOutputPath, GENERIC_WRITE );
    if ( FAILED( hr ) )
    {
        printf( "hr from InitializeFromFilename: %#x\n", hr );
        return hr;
    }

    ComPtr<IWICBitmapEncoder> encoder;
    hr = g_IWICFactory->CreateEncoder( containerFormat, NULL, encoder.GetAddressOf() );
    if ( FAILED( hr ) )
    {
        printf( "hr from create encoder: %#x\n", hr );
        return hr;
    }
    
    hr = encoder->Initialize( fileStream.Get(), WICBitmapEncoderNoCache );
    if ( FAILED( hr ) )
    {
        printf( "initialize encoder: %#x\n", hr );
        return hr;
    }

    UINT count = 0;
    hr = decoder->GetFrameCount( &count );
    if ( FAILED( hr ) )
    {
        printf( "hr from GetFrameCount: %#x\n", hr );
        return hr;
    }

    for ( UINT i = 0; i < count; i++ )
    {
        ComPtr<IWICBitmapFrameDecode> frameDecode;
        hr = decoder->GetFrame( i, frameDecode.GetAddressOf() );
        if ( FAILED( hr ) )
        {
            printf( "hr from GetFrame %d: %#x\n", i, hr );
            return hr;
        }

        ComPtr<IWICBitmapFrameEncode> frameEncode;
        ComPtr<IPropertyBag2> propertyBag;
        hr = encoder->CreateNewFrame( frameEncode.GetAddressOf(), propertyBag.GetAddressOf() );
        if ( FAILED( hr ) )
        {
            printf( "hr from CreateNewFrame: %#x\n", hr );
            return hr;
        }

        PROPBAG2 option = { 0 };
        option.pstrName = L"TiffCompressionMethod";
        VARIANT varValue;
        VariantInit( &varValue );
        varValue.vt = VT_UI1;

        if ( 8 == compressionMethod )
            varValue.bVal = WICTiffCompressionZIP;
        else if ( 5 == compressionMethod )
            varValue.bVal = WICTiffCompressionLZW;
        else if ( 1 == compressionMethod )
            varValue.bVal = WICTiffCompressionNone;
        else
        {
            printf( "invalid compression method %d\n", compressionMethod );
            return E_FAIL;
        }

        hr = propertyBag->Write( 1, &option, &varValue );
        if ( FAILED( hr ) )
        {
            printf( "propertybag->write failed with error %#x\n", hr );
            return hr;
        }

        hr = frameEncode->Initialize( propertyBag.Get() );
        if ( FAILED( hr ) )
        {
            printf( "hr from frameEncode->Initialize: %#x\n", hr );
            return hr;
        }

        ComPtr<IWICMetadataBlockReader> blockReader;
        hr = frameDecode->QueryInterface( IID_IWICMetadataBlockReader, (void **) blockReader.GetAddressOf() );
        if ( FAILED( hr ) )
        {
            printf( "qi of blockReader: %#x\n", hr );
            return hr;
        }

        ComPtr<IWICMetadataBlockWriter> blockWriter;
        hr = frameEncode->QueryInterface( IID_IWICMetadataBlockWriter, (void **) blockWriter.GetAddressOf() );
        if ( FAILED( hr ) )
        {
            printf( "qi of blockWriter: %#x\n", hr );
            return hr;
        }

        hr = blockWriter->InitializeFromBlockReader( blockReader.Get() );
        if ( FAILED( hr ) )
        {
            printf( "hr from InitializeFromBlockReader: %#x\n", hr );
            return hr;
        }

        hr = frameEncode->WriteSource( static_cast<IWICBitmapSource*> ( frameDecode.Get() ), NULL );
        if ( FAILED( hr ) )
        {
            printf( "frameencode->writesource: %#x\n", hr );
            return hr;
        }
   
        hr = frameEncode->Commit();
        if ( FAILED( hr ) )
        {
            printf( "hr from frameencode->commit: %#x\n", hr );
            return hr;
        }
    }
    
    hr = encoder->Commit();
    if ( FAILED( hr ) )
    {
        printf( "hr from encoder->commit() %#x\n", hr );
        return hr;
    }
   
    hr = fileStream->Commit( STGC_DEFAULT );
    if ( FAILED( hr ) )
    {
        printf( "hr from filestream->commit() %#x\n", hr );
        return hr;
    }

    return hr;
} //SetTiffCompression

HRESULT GetTiffCompression( WCHAR const * pwcPath, DWORD * pCompression )
{
    *pCompression = 0;
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = g_IWICFactory->CreateDecoderFromFilename( pwcPath, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf() );
    if ( FAILED( hr ) )
    {
        printf( "hr from create decoder: %#x\n", hr );
        return hr;
    }

    GUID containerFormat;
    hr = decoder->GetContainerFormat( &containerFormat );
    if ( FAILED( hr ) )
    {
        printf( "hr from GetContainerFormat: %#x\n", hr );
        return hr;
    }

    if ( GUID_ContainerFormatTiff != containerFormat )
    {
        printf( "container format of the input file isn't TIFF, so exiting early\n" );
        return E_FAIL;
    }

    UINT count = 0;
    hr = decoder->GetFrameCount( &count );
    if ( FAILED( hr ) )
    {
        printf( "hr from GetFrameCount: %#x\n", hr );
        return hr;
    }

    for ( UINT i = 0; i < count; i++ )
    {
        ComPtr<IWICBitmapFrameDecode> frameDecode;
        hr = decoder->GetFrame( i, frameDecode.GetAddressOf() );
        if ( FAILED( hr ) )
        {
            printf( "hr from GetFrame %d: %#x\n", i, hr );
            return hr;
        }

        ComPtr<IWICMetadataQueryReader> queryReader;
        hr = frameDecode->GetMetadataQueryReader( queryReader.GetAddressOf() );
        if ( FAILED( hr ) )
        {
            printf( "hr from GetMetadataQueryReader: %#x\n", hr );
            return hr;
        }

        WCHAR const * compressionName = L"/ifd/{ushort=259}";
        PROPVARIANT value;
        PropVariantInit( &value );
        hr = queryReader->GetMetadataByName( compressionName, &value );
        if ( FAILED( hr ) )
        {
            printf( "hr from getmetadatabyname for compression: %#x\n", hr );
            return hr;
        }

        // just use the compression value from the first frame

        *pCompression = value.uiVal;
        return S_OK;
    }
    
    return E_FAIL; // none of the frames had compression set
} //GetTiffCompression

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

extern "C" int __cdecl wmain( int argc, WCHAR * argv[] )
{
    WCHAR * pwcPath = NULL;
    BOOL infoOnly = false;
    DWORD compressionMethod = 8;
    int iArg = 1;

    while ( iArg < argc )
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
        }
        else
        {
            if ( 0 != pwcPath )
                Usage();

            pwcPath = argv[ iArg ];
        }

       iArg++;
    }

    if ( NULL == pwcPath )
        Usage();

    printf( "input file: %ws\n", pwcPath );

    static WCHAR awcInputPath[ MAX_PATH ];
    WCHAR * pwcResult = _wfullpath( awcInputPath, pwcPath, _countof( awcInputPath ) );
    if ( NULL == pwcResult )
    {
        printf( "can't call _wfullpath on %ws\n", pwcPath );
        return 0;
    }

    static WCHAR awcOutputPath[ MAX_PATH ];
    CreateOutputPath( awcInputPath, awcOutputPath );

    HRESULT hr = CoInitializeEx( NULL, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE );
    if ( FAILED( hr ) )
    {
        printf( "can't coinitializeex: %#x\n", hr );
        return 0;
    }

    hr = CoCreateInstance( CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, __uuidof( IWICImagingFactory ),
                           reinterpret_cast<void **>  ( g_IWICFactory.GetAddressOf() ) );
    if ( FAILED( hr ) )
    {
        printf( "can't initialize wic: %#x\n", hr );
        return 0;
    }

    // Delete any old version of the temporary file

    BOOL ok = DeleteFile( awcOutputPath );

    DWORD currentCompression = 0;
    hr = GetTiffCompression( awcInputPath, &currentCompression );
    if ( FAILED( hr ) )
    {
        printf( "failed to read current compression value: %#x\n", hr );
        return 1;
    }

    printf( "the current compression is %d == %s for file %ws\n", currentCompression, CompressionType( currentCompression ), awcInputPath );

    if ( infoOnly || compressionMethod == currentCompression )
        exit( 0 );

    hr = SetTiffCompression( awcInputPath, awcOutputPath, compressionMethod );
 
    if ( SUCCEEDED( hr ) && !infoOnly )
    {
        DWORD sizePhoto = GetSize( awcInputPath );
        DWORD sizeOutput = GetSize( awcOutputPath );

        // 1) rename the original file to a safety name

        static WCHAR awcSafety[ MAX_PATH ];
        wcscpy( awcSafety, awcInputPath );
        wcscat( awcSafety, L"-saved" );
        ok = MoveFile( awcInputPath, awcSafety );
        if ( !ok )
        {
            printf( "can't rename the original file, error %d\n", GetLastError() );
            exit( 1 );
        }

        // rename the compressed file as the origianal file

        ok = MoveFile( awcOutputPath, awcInputPath );
        if ( !ok )
        {
            printf( "can't rename new file to the original file name, error %d\n", GetLastError() );
            exit( 1 );
        }

        // delete the (renamed) original file

        ok = DeleteFile( awcSafety );
        if ( !ok )
        {
            printf( "can't delete the saved original file, error %d\n", GetLastError() );
            exit( 1 );
        }

        printf( "original file size " );
        PrintNumberWithCommas( sizePhoto );
        printf( " new size " );
        PrintNumberWithCommas( sizeOutput );
        printf( "\n" );
    }
    
    g_IWICFactory.Reset();
    CoUninitialize();

    return 0;
} //wmain

