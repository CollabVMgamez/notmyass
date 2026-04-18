//======================================================================
//
// NotMyASS.c
//
// Cross-platform entrypoint. Linux uses a small X11 GUI; Windows still shows
// a tiny native GUI control window.
//
//======================================================================
#include <signal.h>
#include <stdio.h>

#ifdef _WIN32

#include <windows.h>
#include "myass.h"
#include "IOCTLCMD.H"

static const char kCrashReasonHex[] = "0x6D79617373";
static const char *kWindowTitle = "Not My ASS";
static const char *kCrashHint = "Crash requested: " kCrashReasonHex;

#define ID_BTN_CRASH 2000
#define ID_BTN_EXIT  2001

static void
trigger_crash( void )
{
    DWORD bytesReturned = 0;
    DWORD error = 0;

    MessageBoxA(
        NULL,
        kCrashHint,
        kWindowTitle,
        MB_OK | MB_ICONWARNING
    );

    if ( SysHandle == INVALID_HANDLE_VALUE ) {
        if ( !LoadDeviceDriver( SYS_NAME, SYS_FILE, &SysHandle, &error ) ) {
            raise( SIGSEGV );
        }
    }

    if ( !DeviceIoControl(
            SysHandle,
            IOCTL_IRQL,
            NULL,
            0,
            NULL,
            0,
            &bytesReturned,
            NULL ) ) {
        raise( SIGSEGV );
    }
}

static LRESULT CALLBACK
window_proc(
    HWND hwnd,
    UINT msg,
    WPARAM w_param,
    LPARAM l_param
    )
{
    switch ( msg ) {
    case WM_CREATE:
        CreateWindowA(
            "BUTTON",
            "&Crash",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, 30, 120, 28,
            hwnd,
            (HMENU) ID_BTN_CRASH,
            NULL,
            NULL );
        CreateWindowA(
            "BUTTON",
            "Exit",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, 70, 120, 28,
            hwnd,
            (HMENU) ID_BTN_EXIT,
            NULL,
            NULL );
        return 0;

    case WM_COMMAND:
        switch ( LOWORD( w_param ) ) {
        case ID_BTN_CRASH:
            trigger_crash();
            return 0;
        case ID_BTN_EXIT:
            DestroyWindow( hwnd );
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow( hwnd );
        return 0;

    case WM_DESTROY:
        if ( SysHandle != INVALID_HANDLE_VALUE ) {
            UnloadDeviceDriver( SYS_NAME );
            SysHandle = INVALID_HANDLE_VALUE;
        }
        PostQuitMessage( 0 );
        return 0;
    }

    return DefWindowProc( hwnd, msg, w_param, l_param );
}

int
WINAPI
WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR     lpCmdLine,
    int       nCmdShow
    )
{
    HWND hwnd;
    MSG msg;
    WNDCLASSA wc = { 0 };
    int argc = 0;
    LPWSTR *args;
    int i;

    args = CommandLineToArgvW( GetCommandLineW(), &argc );
    for ( i = 1; args && i < argc; i++ ) {
        if ( ( args[ i ][0] == L'-' && args[ i ][1] == L'c' ) ||
             ( args[ i ][0] == L'/' && args[ i ][1] == L'c' ) ) {
            trigger_crash();
            return 1;
        }
    }
    LocalFree( args );

    wc.lpfnWndProc = window_proc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "NotMyASSWin";
    wc.hbrBackground = (HBRUSH) ( COLOR_BTNFACE + 1 );
    wc.hCursor = LoadCursorA( NULL, IDC_ARROW );
    RegisterClassA( &wc );

    hwnd = CreateWindowExA(
        0,
        "NotMyASSWin",
        kWindowTitle,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        200, 160,
        NULL,
        NULL,
        hInstance,
        NULL
    );
    if ( !hwnd ) {
        return 1;
    }
    ShowWindow( hwnd, nCmdShow );
    UpdateWindow( hwnd );

    while ( GetMessageA( &msg, NULL, 0, 0 ) ) {
        TranslateMessage( &msg );
        DispatchMessageA( &msg );
    }

    return ( int ) msg.wParam;
}

#else
#include <stdio.h>

#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#ifndef __has_include
#define __has_include(x) 0
#endif

#if __has_include(<X11/Xlib.h>) && __has_include(<X11/Xutil.h>)
#define HAS_X11_GUI 1
#ifdef FORCE_NO_X11_GUI
#undef HAS_X11_GUI
#define HAS_X11_GUI 0
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#else
#define HAS_X11_GUI 0
#endif

static const char kCrashReasonHex[] = "0x6D79617373";
static const char *kDriverPath = "/dev/myass";
static const char *kWindowTitle = "Not My ASS";
static const char *kModuleName = "myass.ko";
static const char *kDkmsModuleName = "myass";
static const char *kDkmsModuleVersion = "1.0";
static const char *kDkmsSrcBase = "/usr/src";
static const char *kDkmsConfName = "dkms.conf";

#define GUI_BTN_CRASH_X 25
#define GUI_BTN_CRASH_Y 30
#define GUI_BTN_CRASH_W 90
#define GUI_BTN_CRASH_H 30
#define GUI_BTN_EXIT_X  25
#define GUI_BTN_EXIT_Y  70
#define GUI_BTN_EXIT_W  90
#define GUI_BTN_EXIT_H  30

#define SHELL_CMD_MAX 8192
#define INSTALL_CONFIRM_TEXT "INSTALL MYASS"

static int command_exists( const char *command );

static int
file_exists( const char *path )
{
    return access( path, F_OK ) == 0;
}

static void
print_usage( const char *prog )
{
    const char *exe_name = prog ? strrchr( prog, '/' ) : NULL;
    if ( exe_name ) {
        exe_name++;
    } else {
        exe_name = prog ? prog : "myass";
    }

    fprintf(
        stderr,
        "%s - Linux Not My ASS (for local testing)\n"
        "Usage:\n"
        "  %s [options]\n"
        "\n"
        "Options:\n"
        "  -h, --help, /?          Show this help text and exit.\n"
        "  -crash, --crash          Trigger crash request immediately.\n"
        "  -install-driver,\n"
        "  /install-driver,\n"
        "  --install-driver         Persistently install module and then load it.\n"
        "                           Requires root and explicit confirmation.\n"
        "\n",
        exe_name,
        exe_name
    );
}

static int
run_shell( const char *cmd )
{
    int rc;
    int status;

    rc = system( cmd );
    if ( rc == -1 ) {
        fprintf( stderr, "Failed to execute command: %s\n", strerror( errno ) );
        return 0;
    }

    if ( !WIFEXITED( rc ) ) {
        fprintf( stderr, "Command did not terminate normally: %s\n", cmd );
        return 0;
    }

    status = WEXITSTATUS( rc );
    if ( status != 0 ) {
        fprintf( stderr, "Command failed (exit=%d): %s\n", status, cmd );
        return 0;
    }

    return 1;
}

static int
run_shell_capture_stderr( const char *cmd )
{
    char output[ SHELL_CMD_MAX ];
    char full_cmd[ SHELL_CMD_MAX ];
    FILE *pipe;
    int rc;
    int status;
    int saw_output = 0;

    if ( !cmd ) {
        return 0;
    }

    snprintf( full_cmd, sizeof( full_cmd ), "%s 2>&1", cmd );
    pipe = popen( full_cmd, "r" );
    if ( !pipe ) {
        fprintf( stderr, "Failed to run command: %s\n", cmd );
        return 0;
    }

    while ( fgets( output, sizeof( output ), pipe ) ) {
        saw_output = 1;
        fputs( output, stderr );
    }

    rc = pclose( pipe );
    if ( rc == -1 ) {
        fprintf( stderr, "Failed to complete command: %s\n", cmd );
        return 0;
    }

    if ( !WIFEXITED( rc ) ) {
        fprintf( stderr, "Command did not terminate normally: %s\n", cmd );
        return 0;
    }

    status = WEXITSTATUS( rc );
    if ( status != 0 ) {
        if ( !saw_output ) {
            fprintf( stderr, "Command failed (exit=%d): %s\n", status, cmd );
        }
        return 0;
    }

    return 1;
}

static int
module_matches_running_kernel( const char *module_path )
{
    struct utsname uts;
    char vermagic[ 256 ];
    char cmd[ SHELL_CMD_MAX ];
    FILE *pipe = NULL;
    int matched = 1;
    char *nl;

    if ( !module_path || !file_exists( module_path ) ) {
        return 0;
    }

    if ( !command_exists( "modinfo" ) ) {
        return 1;
    }

    if ( uname( &uts ) != 0 ) {
        fprintf( stderr, "Could not read kernel release while checking module compatibility.\n" );
        return 1;
    }

    snprintf( cmd, sizeof( cmd ), "modinfo -F vermagic \"%s\" 2>/dev/null", module_path );
    pipe = popen( cmd, "r" );
    if ( !pipe ) {
        fprintf( stderr, "Could not check module compatibility for %s\n", module_path );
        return 1;
    }

    if ( fgets( vermagic, sizeof( vermagic ), pipe ) ) {
        nl = strpbrk( vermagic, "\r\n" );
        if ( nl ) {
            *nl = '\0';
        }

        if ( !strstr( vermagic, uts.release ) ) {
            fprintf(
                stderr,
                "Module kernel signature mismatch for %s\n"
                "  Module: %s\n"
                "  Kernel: %s\n"
                "Rebuild the module on this machine with matching kernel headers.\n",
                module_path,
                vermagic,
                uts.release
            );
            matched = 0;
        }
    } else {
        fprintf(
            stderr,
            "Could not read vermagic from %s; treating as potentially compatible.\n",
            module_path
        );
    }

    if ( pclose( pipe ) == -1 ) {
        fprintf( stderr, "Could not finish vermagic check for %s.\n", module_path );
    }

    return matched;
}

static int
command_exists( const char *command )
{
    char probe[ SHELL_CMD_MAX ];

    if ( !command || !command[ 0 ] ) {
        return 0;
    }

    snprintf( probe, sizeof( probe ), "command -v \"%s\" >/dev/null 2>&1", command );
    if ( system( probe ) == 0 ) {
        return 1;
    }

    return 0;
}

static int
insmod_module( const char *path )
{
    char cmd[ SHELL_CMD_MAX ];

    if ( !path || !file_exists( path ) ) {
        return 0;
    }

    if ( !module_matches_running_kernel( path ) ) {
        return 0;
    }

    snprintf( cmd, sizeof( cmd ), "insmod \"%s\"", path );
    return run_shell_capture_stderr( cmd );
}

static void
get_executable_dir( char *out, size_t out_size )
{
    ssize_t exe_len;

    exe_len = readlink( "/proc/self/exe", out, out_size - 1 );
    if ( exe_len < 0 || exe_len >= ( ssize_t ) ( out_size - 1 ) ) {
        snprintf( out, out_size, "." );
        return;
    }

    out[ exe_len ] = '\0';
    {
        char *slash;
        slash = strrchr( out, '/' );
        if ( !slash ) {
            snprintf( out, out_size, "." );
            return;
        }

        *slash = '\0';
        if ( out[ 0 ] == '\0' ) {
            out[ 0 ] = '/';
            out[ 1 ] = '\0';
        }
    }
}

static int
prompt_for_explicit_permission( void )
{
    char response[ 32 ];

    if ( !isatty( STDIN_FILENO ) ) {
        fprintf(
            stderr,
            "Install mode requires explicit permission and a tty. "
            "Rerun from an interactive terminal.\n"
        );
        return 0;
    }

    fprintf(
        stderr,
        "Explicitly type '" INSTALL_CONFIRM_TEXT
        "' (all caps, no quotes) to permanently install myass module: "
    );
    fflush( stderr );

    if ( !fgets( response, sizeof( response ), stdin ) ) {
        return 0;
    }

    response[ strcspn( response, "\r\n" ) ] = '\0';
    if ( strcmp( response, INSTALL_CONFIRM_TEXT ) != 0 ) {
        fprintf( stderr, "Permission denied. Installation aborted.\n" );
        return 0;
    }

    return 1;
}

static int
copy_file_if_present( const char *src, const char *dst )
{
    int src_fd = -1;
    int dst_fd = -1;
    char buf[ 4096 ];
    ssize_t read_bytes;
    ssize_t write_bytes;
    ssize_t written_total;
    ssize_t left;

    if ( !src || !dst ) {
        return 0;
    }

    src_fd = open( src, O_RDONLY );
    if ( src_fd < 0 ) {
        fprintf( stderr, "Cannot read source module %s: %s\n", src, strerror( errno ) );
        return 0;
    }

    dst_fd = open( dst, O_WRONLY | O_CREAT | O_TRUNC, 0644 );
    if ( dst_fd < 0 ) {
        fprintf( stderr, "Cannot write destination module %s: %s\n", dst, strerror( errno ) );
        close( src_fd );
        return 0;
    }

    while ( ( read_bytes = read( src_fd, buf, sizeof( buf ) ) ) > 0 ) {
        written_total = 0;
        left = read_bytes;
        while ( written_total < read_bytes ) {
            write_bytes = write(
                dst_fd,
                buf + written_total,
                ( size_t ) left
            );
            if ( write_bytes <= 0 ) {
                if ( errno == EINTR ) {
                    continue;
                }
                fprintf(
                    stderr,
                    "Write failed while copying %s -> %s: %s\n",
                    src,
                    dst,
                    strerror( errno )
                );
                close( src_fd );
                close( dst_fd );
                return 0;
            }
            written_total += write_bytes;
            left = read_bytes - written_total;
        }
    }

    close( src_fd );
    close( dst_fd );

    if ( read_bytes < 0 ) {
        fprintf(
            stderr,
            "Read failed while copying %s -> %s: %s\n",
            src,
            dst,
            strerror( errno )
        );
        return 0;
    }

    return 1;
}

static int
write_text_file( const char *path, const char *content )
{
    int fd = -1;
    ssize_t content_len;
    ssize_t written_total;
    ssize_t left;
    ssize_t chunk;

    if ( !path || !content ) {
        return 0;
    }

    fd = open( path, O_WRONLY | O_CREAT | O_TRUNC, 0644 );
    if ( fd < 0 ) {
        fprintf( stderr, "Cannot write %s: %s\n", path, strerror( errno ) );
        return 0;
    }

    content_len = ( ssize_t ) strlen( content );
    written_total = 0;
    left = content_len;
    while ( written_total < content_len ) {
        chunk = write( fd, content + written_total, ( size_t ) left );
        if ( chunk < 0 ) {
            if ( errno == EINTR ) {
                continue;
            }
            fprintf( stderr, "Write failed for %s: %s\n", path, strerror( errno ) );
            close( fd );
            return 0;
        }
        written_total += chunk;
        left = content_len - written_total;
    }

    if ( close( fd ) != 0 ) {
        fprintf( stderr, "Could not close %s after write: %s\n", path, strerror( errno ) );
        return 0;
    }

    return 1;
}

static int
find_bundled_module( char *out, size_t out_size )
{
    char exe_dir[ PATH_MAX ];

    get_executable_dir( exe_dir, sizeof( exe_dir ) );
    snprintf( out, out_size, "%s/%s", exe_dir, kModuleName );
    if ( file_exists( out ) ) {
        return 1;
    }

    snprintf( out, out_size, "%s/../sys/%s", exe_dir, kModuleName );
    if ( file_exists( out ) ) {
        return 1;
    }

    return 0;
}

static int
find_bundled_source_file( const char *filename, char *out, size_t out_size )
{
    char exe_dir[ PATH_MAX ];

    get_executable_dir( exe_dir, sizeof( exe_dir ) );

    snprintf( out, out_size, "%s/%s", exe_dir, filename );
    if ( file_exists( out ) ) {
        return 1;
    }

    snprintf( out, out_size, "%s/../sys/%s", exe_dir, filename );
    if ( file_exists( out ) ) {
        return 1;
    }

    return 0;
}

static int
build_dkms_conf( const char *src_dir )
{
    char conf_path[ PATH_MAX ];
    char conf_body[ 512 ];

    if ( !src_dir ) {
        return 0;
    }

    snprintf(
        conf_path,
        sizeof( conf_path ),
        "%s/%s",
        src_dir,
        kDkmsConfName
    );

    snprintf(
        conf_body,
        sizeof( conf_body ),
        "PACKAGE_NAME=\"%s\"\n"
        "PACKAGE_VERSION=\"%s\"\n"
        "AUTOINSTALL=\"yes\"\n"
        "\n"
        "BUILT_MODULE_NAME[0]=\"%s\"\n"
        "BUILT_MODULE_LOCATION[0]=\".\"\n"
        "DEST_MODULE_NAME[0]=\"%s\"\n"
        "DEST_MODULE_LOCATION[0]=\"/updates/dkms\"\n",
        kDkmsModuleName,
        kDkmsModuleVersion,
        kDkmsModuleName,
        kDkmsModuleName
    );

    if ( !write_text_file( conf_path, conf_body ) ) {
        fprintf( stderr, "Failed writing DKMS config %s.\n", conf_path );
        return 0;
    }

    return 1;
}

static void
cleanup_dkms_workspace( const char *path )
{
    char cmd[ SHELL_CMD_MAX ];

    if ( !path || !path[ 0 ] ) {
        return;
    }

    snprintf( cmd, sizeof( cmd ), "rm -rf -- \"%s\" >/dev/null 2>&1", path );
    run_shell( cmd );
}

static int
install_with_dkms( void )
{
    struct utsname uts;
    char cmd[ SHELL_CMD_MAX ];
    char src_module[ PATH_MAX ];
    char src_makefile[ PATH_MAX ];
    char dkms_src_root[ PATH_MAX ];
    char module_src[ PATH_MAX ];
    char makefile_src[ PATH_MAX ];
    int installed = 0;

    if ( !command_exists( "dkms" ) ) {
        fprintf( stderr, "dkms not found; falling back to direct module install.\n" );
        return 0;
    }

    if ( !find_bundled_source_file( "myass.c", src_module, sizeof( src_module ) ) ) {
        fprintf(
            stderr,
            "Driver source module not found. "
            "Run from repo layout (exe/myass with ../sys/myass.c source).\n"
        );
        return 0;
    }

    if ( !find_bundled_source_file( "Makefile", src_makefile, sizeof( src_makefile ) ) ) {
        fprintf(
            stderr,
            "Kernel Makefile not found for DKMS install. "
            "Run from repo layout with bundled sys/Makefile.\n"
        );
        return 0;
    }

    if ( uname( &uts ) != 0 ) {
        fprintf( stderr, "Could not read kernel release.\n" );
        return 0;
    }

    snprintf(
        dkms_src_root,
        sizeof( dkms_src_root ),
        "%s/%s-%s",
        kDkmsSrcBase,
        kDkmsModuleName,
        kDkmsModuleVersion
    );

    snprintf(
        cmd,
        sizeof( cmd ),
        "rm -rf \"%s\"",
        dkms_src_root
    );
    if ( !run_shell( cmd ) ) {
        fprintf( stderr, "Could not clear previous DKMS source directory %s.\n", dkms_src_root );
        return 0;
    }

    snprintf( cmd, sizeof( cmd ), "mkdir -p \"%s\"", dkms_src_root );
    if ( !run_shell( cmd ) ) {
        fprintf( stderr, "Could not create DKMS source directory %s.\n", dkms_src_root );
        return 0;
    }

    snprintf(
        module_src,
        sizeof( module_src ),
        "%s/%s",
        dkms_src_root,
        "myass.c"
    );
    snprintf(
        makefile_src,
        sizeof( makefile_src ),
        "%s/Makefile",
        dkms_src_root
    );

    if ( !copy_file_if_present( src_module, module_src ) ||
         !copy_file_if_present( src_makefile, makefile_src ) ) {
        cleanup_dkms_workspace( dkms_src_root );
        return 0;
    }

    if ( !build_dkms_conf( dkms_src_root ) ) {
        cleanup_dkms_workspace( dkms_src_root );
        return 0;
    }

    snprintf(
        cmd,
        sizeof( cmd ),
        "dkms remove -m %s -v %s --all >/dev/null 2>&1 || true",
        kDkmsModuleName,
        kDkmsModuleVersion
    );
    run_shell( cmd );

    fprintf(
        stderr,
        "Installing myass via DKMS for kernel %s (this will build on this host).\n",
        uts.release
    );

    snprintf(
        cmd,
        sizeof( cmd ),
        "dkms add -m %s -v %s",
        kDkmsModuleName,
        kDkmsModuleVersion
    );
    if ( !run_shell_capture_stderr( cmd ) ) {
        cleanup_dkms_workspace( dkms_src_root );
        return 0;
    }

    snprintf(
        cmd,
        sizeof( cmd ),
        "dkms build -m %s -v %s -k %s",
        kDkmsModuleName,
        kDkmsModuleVersion,
        uts.release
    );
    if ( !run_shell_capture_stderr( cmd ) ) {
        fprintf(
            stderr,
            "DKMS build failed. Verify kernel headers/tools and rerun with verbose output.\n"
        );
        cleanup_dkms_workspace( dkms_src_root );
        return 0;
    }

    snprintf(
        cmd,
        sizeof( cmd ),
        "dkms install -m %s -v %s -k %s",
        kDkmsModuleName,
        kDkmsModuleVersion,
        uts.release
    );
    if ( !run_shell_capture_stderr( cmd ) ) {
        cleanup_dkms_workspace( dkms_src_root );
        return 0;
    }

    if ( command_exists( "modprobe" ) ) {
        if ( run_shell_capture_stderr( "modprobe myass" ) ) {
            installed = 1;
            goto done;
        }
        fprintf(
            stderr,
            "modprobe myass after DKMS install failed; will try direct module load.\n"
        );
    }

    snprintf(
        cmd,
        sizeof( cmd ),
        "/lib/modules/%s/updates/dkms/myass.ko",
        uts.release
    );
    installed = insmod_module( cmd );

done:
    cleanup_dkms_workspace( dkms_src_root );
    return installed;
}

static int
install_driver_permanently( void )
{
    struct utsname uts;
    char kernel_release[ PATH_MAX ];
    char src_module[ PATH_MAX ];
    char dst_dir[ PATH_MAX ];
    char dst_module[ PATH_MAX ];
    char cmd[ SHELL_CMD_MAX ];

    if ( geteuid() != 0 ) {
        fprintf( stderr, "Persistent install requires root privileges.\n" );
        return 0;
    }

    if ( !prompt_for_explicit_permission() ) {
        return 0;
    }

    if ( uname( &uts ) != 0 ) {
        fprintf( stderr, "Could not read kernel release.\n" );
        return 0;
    }

    if ( command_exists( "dkms" ) ) {
        fprintf( stderr, "Attempting DKMS-based install for persistent myass driver.\n" );
        if ( install_with_dkms() ) {
            return 1;
        }
        fprintf(
            stderr,
            "DKMS install failed, trying legacy direct module install fallback.\n"
        );
    } else {
        fprintf( stderr, "dkms not found; using legacy direct install.\n" );
    }

    if ( !find_bundled_module( src_module, sizeof( src_module ) ) ) {
        fprintf(
            stderr,
            "Bundled module not found. Rebuild using `make` and "
            "run with `package/` output available.\n"
        );
        return 0;
    }

    snprintf( kernel_release, sizeof( kernel_release ), "%s", uts.release );
    snprintf(
        dst_dir,
        sizeof( dst_dir ),
        "/lib/modules/%s/extra",
        kernel_release
    );
    if ( !command_exists( "mkdir" ) ) {
        fprintf( stderr, "mkdir utility is required to install module files.\n" );
        return 0;
    }
    snprintf( cmd, sizeof( cmd ), "mkdir -p \"%s\" >/dev/null 2>&1", dst_dir );
    if ( !run_shell( cmd ) ) {
        return 0;
    }

    snprintf( dst_module, sizeof( dst_module ), "%s/%s", dst_dir, kModuleName );
    if ( !copy_file_if_present( src_module, dst_module ) ) {
        fprintf(
            stderr,
            "Could not copy module to %s. Ensure destination path is writable.\n",
            dst_module
        );
        return 0;
    }

    snprintf( cmd, sizeof( cmd ), "depmod -a %s", kernel_release );
    if ( command_exists( "depmod" ) ) {
        if ( !run_shell( cmd ) ) {
            fprintf(
                stderr,
                "depmod failed; continuing anyway to direct module load.\n"
            );
        }
    } else {
        fprintf( stderr, "depmod not found; skipping dependency regeneration.\n" );
    }

    snprintf(
        cmd,
        sizeof( cmd ),
        "chmod 0644 \"%s\" >/dev/null 2>&1",
        dst_module
    );
    if ( command_exists( "chmod" ) ) {
        run_shell( cmd );
    }

    if ( command_exists( "modprobe" ) ) {
        if ( run_shell_capture_stderr( "modprobe myass" ) ) {
            return 1;
        }
        fprintf(
            stderr,
            "modprobe myass failed, falling back to direct insmod.\n"
        );
    } else {
        fprintf( stderr, "modprobe not found; trying direct insmod fallback.\n" );
    }

    snprintf( cmd, sizeof( cmd ), "insmod \"%s\"", dst_module );
    return run_shell_capture_stderr( cmd );
}

#if HAS_X11_GUI
typedef struct {
    int x;
    int y;
    int w;
    int h;
} rect_t;

static int
point_in_rect( int x, int y, rect_t rect )
{
    return x >= rect.x && x < rect.x + rect.w &&
           y >= rect.y && y < rect.y + rect.h;
}

static void
draw_button(
    Display *display,
    Window window,
    GC gc,
    rect_t btn,
    const char *text
    )
{
    XDrawRectangle( display, window, gc, btn.x, btn.y, btn.w, btn.h );
    XDrawString(
        display,
        window,
        gc,
        btn.x + 12,
        btn.y + 19,
        text,
        (int) strlen( text )
    );
}

static void
draw_linux_gui(
    Display *display,
    Window window,
    GC gc
    )
{
    rect_t crash = {
        GUI_BTN_CRASH_X, GUI_BTN_CRASH_Y,
        GUI_BTN_CRASH_W, GUI_BTN_CRASH_H
    };
    rect_t exit = {
        GUI_BTN_EXIT_X, GUI_BTN_EXIT_Y,
        GUI_BTN_EXIT_W, GUI_BTN_EXIT_H
    };

    XClearWindow( display, window );
    draw_button( display, window, gc, crash, "Crash" );
    draw_button( display, window, gc, exit, "Exit" );
    XDrawString( display, window, gc, 5, 15, "NotMyASS", 8 );
}
#endif

/* Must match MYASS_IOCTL_CRASH in sys/myass.c */
#ifndef _IOC_TYPECHECK
#define _IOC_TYPECHECK(t) (sizeof(t))
#endif
#define MYASS_IOCTL_CRASH _IO('M', 0x06)

static int
load_bundled_modules( void )
{
    char exe_dir[ PATH_MAX ];
    char candidate[ PATH_MAX ];

    get_executable_dir( exe_dir, sizeof( exe_dir ) );

    snprintf( candidate, sizeof( candidate ), "%s/%s", exe_dir, kModuleName );
    if ( insmod_module( candidate ) ) {
        return 1;
    }

    snprintf(
        candidate,
        sizeof( candidate ),
        "%s/../sys/%s",
        exe_dir,
        kModuleName
    );
    if ( insmod_module( candidate ) ) {
        return 1;
    }

    return 0;
}

static int
ensure_driver_loaded( int install_if_missing )
{
    if ( install_if_missing ) {
        fprintf( stderr, "Attempting explicit persistent install...\n" );
        if ( install_driver_permanently() ) {
            return access( kDriverPath, F_OK ) == 0;
        }
        fprintf(
            stderr,
            "Persistent install did not complete, falling back to transient load path.\n"
        );
    }

    if ( access( kDriverPath, F_OK ) == 0 ) {
        return 1;
    }

    fprintf( stderr, "Driver not loaded yet. Attempting to load myass module...\n" );

    if ( command_exists( "modprobe" ) ) {
        if ( run_shell_capture_stderr( "modprobe myass" ) ) {
            return access( kDriverPath, F_OK ) == 0;
        }
        fprintf(
            stderr,
            "modprobe myass failed; trying direct insmod path fallback.\n"
        );
    }

    if ( load_bundled_modules() ) {
        return access( kDriverPath, F_OK ) == 0;
    }

    {
        struct utsname uts;
        char path[ SHELL_CMD_MAX ];

        if ( uname( &uts ) == 0 ) {
            snprintf(
                path,
                sizeof( path ),
                "/lib/modules/%s/kernel/drivers/myass/myass.ko",
                uts.release
            );
            if ( insmod_module( path ) ) {
                return access( kDriverPath, F_OK ) == 0;
            }

            snprintf( path, sizeof( path ), "/lib/modules/%s/extra/myass.ko", uts.release );
            if ( insmod_module( path ) ) {
                return access( kDriverPath, F_OK ) == 0;
            }
        }
    }

    if ( insmod_module( "myass.ko" ) ) {
        return access( kDriverPath, F_OK ) == 0;
    }

    fprintf( stderr, "Could not auto-load myass module. Run as root or install module manually.\n" );
    return 0;
}

static void
trigger_crash( void )
{
    int fd;

    fprintf( stderr, "Crash requested. reason: %s\n", kCrashReasonHex );

    fd = open( kDriverPath, O_WRONLY );
    if ( fd >= 0 ) {
        ioctl( fd, MYASS_IOCTL_CRASH, NULL );
        close( fd );
    }

    raise( SIGSEGV );
}

int
main( int argc, char **argv )
{
	int i;
	int wants_cli = 0;
	int show_help = 0;
	int install_driver = 0;

	for ( i = 1; i < argc; i++ ) {
		if ( strcmp( argv[ i ], "/crash" ) == 0 ||
			 strcmp( argv[ i ], "-crash" ) == 0 ||
			 strcmp( argv[ i ], "--crash" ) == 0 ) {
			wants_cli = 1;
			continue;
		}
		if ( strcmp( argv[ i ], "-h" ) == 0 ||
			 strcmp( argv[ i ], "--help" ) == 0 ||
			 strcmp( argv[ i ], "/?" ) == 0 ) {
			show_help = 1;
		}
		if ( strcmp( argv[ i ], "/install-driver" ) == 0 ||
			 strcmp( argv[ i ], "-install-driver" ) == 0 ||
			 strcmp( argv[ i ], "--install-driver" ) == 0 ) {
			install_driver = 1;
		}
	}

	if ( show_help ) {
		print_usage( argv[ 0 ] );
		return 0;
	}

	if ( wants_cli ) {
		if ( !ensure_driver_loaded( install_driver ) ) {
			fprintf( stderr, "Driver not available; continuing to crash attempt.\n" );
		}
		trigger_crash();
		return EXIT_FAILURE;
	}

	if ( !ensure_driver_loaded( install_driver ) ) {
		return 1;
	}
	fprintf( stderr, "Launching %s GUI...\n", kWindowTitle );

	{
#if HAS_X11_GUI
			Display *display;
			int screen;
			Window window;
		XEvent event;
		GC gc;
		Atom delete_atom;
		int should_run = 1;
		rect_t crash_btn = {
			GUI_BTN_CRASH_X, GUI_BTN_CRASH_Y,
			GUI_BTN_CRASH_W, GUI_BTN_CRASH_H
		};
		rect_t exit_btn = {
			GUI_BTN_EXIT_X, GUI_BTN_EXIT_Y,
			GUI_BTN_EXIT_W, GUI_BTN_EXIT_H
		};

		display = XOpenDisplay( NULL );
		if ( !display ) {
			fprintf( stderr, "No X11 display found. Use /crash to trigger CLI mode.\n" );
			return 1;
		}

		screen = DefaultScreen( display );
		window = XCreateSimpleWindow(
			display,
			RootWindow( display, screen ),
			20,
			20,
			200,
			130,
			1,
			BlackPixel( display, screen ),
			WhitePixel( display, screen )
		);

		XStoreName( display, window, kWindowTitle );
		XSelectInput(
			display,
			window,
			ExposureMask | ButtonPressMask | StructureNotifyMask
		);
		delete_atom = XInternAtom( display, "WM_DELETE_WINDOW", False );
			XSetWMProtocols( display, window, &delete_atom, 1 );

		gc = XCreateGC( display, window, 0, NULL );
		XSetForeground( display, gc, BlackPixel( display, screen ) );
		XMapRaised( display, window );

		while ( should_run ) {
			XNextEvent( display, &event );
			if ( event.type == Expose ) {
				draw_linux_gui( display, window, gc );
			} else if ( event.type == ButtonPress ) {
				if ( point_in_rect(
						event.xbutton.x,
						event.xbutton.y,
						crash_btn ) ) {
					trigger_crash();
					continue;
				}
				if ( point_in_rect(
						event.xbutton.x,
						event.xbutton.y,
						exit_btn ) ) {
					should_run = 0;
				}
			} else if ( event.type == ClientMessage &&
						( Atom ) event.xclient.data.l[ 0 ] == delete_atom ) {
				should_run = 0;
			}
		}

		XDestroyWindow( display, window );
		XFreeGC( display, gc );
		XCloseDisplay( display );
#else
		fprintf(
			stderr,
			"X11 headers not available at build time. "
			"Use /crash for CLI mode.\n"
		);
		return 1;
#endif
	}
	return 0;
}

#endif
