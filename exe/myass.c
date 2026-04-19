//======================================================================
//
// NotMyASS.c
//
// Cross-platform entrypoint. Linux uses a small X11 GUI; Windows still shows
// a tiny native GUI control window.
//
//======================================================================
#include <signal.h>
#include <stdarg.h>
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
#include <ctype.h>
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
static const char *kSysrqPath = "/proc/sysrq-trigger";
static const char *kDefaultCrashReason = "0x6D79617373";
static const char *kModuleName = "myass.ko";
static const char *kDkmsModuleName = "myass";
static const char *kDkmsModuleVersion = "1.1";
static const char *kDkmsSrcBase = "/usr/src";
static const char *kDkmsConfName = "dkms.conf";
static const char *kModulesLoadPath = "/etc/modules-load.d/myass.conf";
static const char *kOpenrcModulesPath = "/etc/conf.d/modules";

#define GUI_BTN_DRIVER_X 25
#define GUI_BTN_DRIVER_Y 35
#define GUI_BTN_DRIVER_W 170
#define GUI_BTN_DRIVER_H 30
#define GUI_BTN_SYSRQ_X  25
#define GUI_BTN_SYSRQ_Y  75
#define GUI_BTN_SYSRQ_W  170
#define GUI_BTN_SYSRQ_H  30
#define GUI_BTN_KILL_X  25
#define GUI_BTN_KILL_Y  115
#define GUI_BTN_KILL_W  170
#define GUI_BTN_KILL_H  30
#define GUI_BTN_EXIT_X   25
#define GUI_BTN_EXIT_Y   165
#define GUI_BTN_EXIT_W   170
#define GUI_BTN_EXIT_H  30

#define CRASH_REASON_MAX 127

#define STATUS_PREFIX "MYASS_STATUS"
#define ACTION_CONFIRM_TEXT "YES"

typedef enum {
    INSTALL_MODE_ONCE = 0,
    INSTALL_MODE_BOOT
} install_mode_t;

typedef enum {
    POISON_DRIVER = 0,
    POISON_SYSRQ,
    POISON_KILL_INIT
} poison_t;

#define SHELL_CMD_MAX 8192
#define INSTALL_CONFIRM_TEXT "INSTALL MYASS"

static int g_delay_seconds = 0;
static int g_dry_run = 0;
static int g_wants_tty = 1;
static FILE *g_log_file = NULL;
static install_mode_t g_install_mode = INSTALL_MODE_BOOT;
static int g_status_requested = 0;
static int g_uninstall_requested = 0;
static int g_version_requested = 0;

static const char *kInstallModeNames[] = { "once", "boot" };

static int command_exists( const char *command );
static int file_exists( const char *path );
static int run_shell_or_dry( const char *cmd );
static int run_shell_capture_stderr_or_dry( const char *cmd );
static int run_shell( const char *cmd );
static int run_shell_capture_stderr( const char *cmd );
static int open_log_file( const char *path );
static void close_log_file( void );
static void status_kv( const char *key, const char *value );
static void status_kv_int( const char *key, int value );
static void status_print_mode( void );
static int has_tty( void );
static int parse_install_mode( const char *value, install_mode_t *out_mode );
static int confirm_action( const char *action_name );
static int apply_delay( void );
static int module_is_loaded( void );
static int file_contains_word( const char *path, const char *token );
static int has_boot_autoload_marker( void );
static int find_candidate_module_paths(
    const char *kernel_release,
    char *out,
    size_t out_size
);
static int install_with_dkms( int enable_boot_load );
static int install_driver_permanently( int enable_boot_load );
static int uninstall_driver( void );
static int write_text_file( const char *path, const char *content );
static int uninstall_boot_registration( void );
static int unload_loaded_module( void );
static int trigger_driver_crash_with_reason( const char *reason );

static void
trim_whitespace( char *text )
{
    char *end;
    char *start;

    if ( !text ) {
        return;
    }

    start = text;
    while ( *start && isspace( ( unsigned char ) *start ) ) {
        start++;
    }
    if ( start != text ) {
        memmove( text, start, strlen( start ) + 1 );
    }

    end = text + strlen( text );
    while ( end > text && isspace( ( unsigned char ) end[ -1 ] ) ) {
        --end;
    }
    *end = '\0';
}

static void
read_reason_from_stdin( char *out, size_t out_size )
{
    const char *fallback = kDefaultCrashReason;

    if ( !out || out_size == 0 ) {
        return;
    }

    out[ 0 ] = '\0';
    if ( !isatty( STDIN_FILENO ) ) {
        snprintf( out, out_size, "%s", fallback );
        return;
    }

    fprintf( stderr, "Custom crash reason (empty uses %s): ", fallback );
    fflush( stderr );
    if ( !fgets( out, ( int ) out_size, stdin ) ) {
        snprintf( out, out_size, "%s", fallback );
        return;
    }
    trim_whitespace( out );
    if ( out[ 0 ] == '\0' ) {
        snprintf( out, out_size, "%s", fallback );
    }
}

static int
has_tty( void )
{
    return g_wants_tty && isatty( STDIN_FILENO );
}

static int
open_log_file( const char *path )
{
    if ( !path || !path[ 0 ] ) {
        return 0;
    }

    g_log_file = fopen( path, "a" );
    if ( !g_log_file ) {
        fprintf( stderr, "Could not open log path %s: %s\n", path, strerror( errno ) );
        return 0;
    }

    return 1;
}

static void
close_log_file( void )
{
    if ( g_log_file ) {
        fclose( g_log_file );
        g_log_file = NULL;
    }
}

static void
status_kv( const char *key, const char *value )
{
    if ( !key || !value ) {
        return;
    }

    fprintf( stderr, "%s_%s=%s\n", STATUS_PREFIX, key, value );
    if ( g_log_file ) {
        fprintf( g_log_file, "%s_%s=%s\n", STATUS_PREFIX, key, value );
        fflush( g_log_file );
    }
}

static void
status_kv_int( const char *key, int value )
{
    char value_str[ 64 ];
    snprintf( value_str, sizeof( value_str ), "%d", value );
    status_kv( key, value_str );
}

static int
parse_install_mode( const char *value, install_mode_t *out_mode )
{
    if ( !value || !out_mode ) {
        return 0;
    }

    if ( strcmp( value, kInstallModeNames[ INSTALL_MODE_ONCE ] ) == 0 ) {
        *out_mode = INSTALL_MODE_ONCE;
        return 1;
    }

    if ( strcmp( value, kInstallModeNames[ INSTALL_MODE_BOOT ] ) == 0 ) {
        *out_mode = INSTALL_MODE_BOOT;
        return 1;
    }

    return 0;
}

static int
run_shell_or_dry( const char *cmd )
{
    if ( !cmd ) {
        return 0;
    }

    if ( g_dry_run ) {
        fprintf( stderr, "DRY-RUN: %s\n", cmd );
        if ( g_log_file ) {
            fprintf( g_log_file, "DRY-RUN: %s\n", cmd );
            fflush( g_log_file );
        }
        return 1;
    }

    return run_shell( cmd );
}

static int
run_shell_capture_stderr_or_dry( const char *cmd )
{
    if ( !cmd ) {
        return 0;
    }

    if ( g_dry_run ) {
        fprintf( stderr, "DRY-RUN: %s\n", cmd );
        if ( g_log_file ) {
            fprintf( g_log_file, "DRY-RUN: %s\n", cmd );
            fflush( g_log_file );
        }
        return 1;
    }

    return run_shell_capture_stderr( cmd );
}

static int
confirm_action( const char *action_name )
{
    char response[ 32 ];

    if ( g_dry_run ) {
        fprintf( stderr, "%s [dry-run] auto-confirmed.\n", action_name );
        if ( g_log_file ) {
            fprintf( g_log_file, "%s [dry-run] auto-confirmed.\n", action_name );
            fflush( g_log_file );
        }
        return 1;
    }

    if ( !has_tty() ) {
        fprintf(
            stderr,
            "%s requires an interactive terminal for confirmation.\n",
            action_name
        );
        return 0;
    }

    fprintf(
        stderr,
        "Type '%s' (all caps, no quotes) to confirm %s: ",
        ACTION_CONFIRM_TEXT,
        action_name
    );
    fflush( stderr );

    if ( !fgets( response, sizeof( response ), stdin ) ) {
        return 0;
    }

    trim_whitespace( response );
    if ( strcmp( response, ACTION_CONFIRM_TEXT ) != 0 ) {
        fprintf( stderr, "Permission denied.\n" );
        return 0;
    }

    return 1;
}

static int
apply_delay( void )
{
    if ( g_delay_seconds <= 0 ) {
        return 1;
    }

    if ( g_dry_run ) {
        fprintf( stderr, "Dry-run delay: %d seconds skipped.\n", g_delay_seconds );
        return 1;
    }

    sleep( g_delay_seconds );
    return 1;
}

static void
status_print_mode( void )
{
    status_kv( "VERSION", kDkmsModuleVersion );
    status_kv( "INSTALL_MODE", g_install_mode == INSTALL_MODE_BOOT ? "boot" : "once" );
    status_kv_int( "DELAY_SECONDS", g_delay_seconds );
    status_kv_int( "DRY_RUN", g_dry_run );
    status_kv( "DRIVER_LOADED", module_is_loaded() ? "yes" : "no" );
    status_kv( "BOOT_AUTOLOAD", has_boot_autoload_marker() ? "yes" : "no" );
}

static int
module_is_loaded( void )
{
    return file_contains_word( "/proc/modules", kDkmsModuleName );
}

static int
file_contains_word( const char *path, const char *token )
{
    FILE *fp;
    char line[ 4096 ];
    size_t token_len;
    const char *hit;
    int found = 0;

    if ( !path || !token || !token[ 0 ] ) {
        return 0;
    }

    token_len = strlen( token );
    fp = fopen( path, "r" );
    if ( !fp ) {
        return 0;
    }

    while ( fgets( line, ( int ) sizeof( line ), fp ) ) {
        char *cursor = line;

        while ( ( hit = strstr( cursor, token ) ) != NULL ) {
            int before_ok = ( hit == line ) ||
                isspace( ( unsigned char ) hit[ -1 ] ) ||
                hit[ -1 ] == '\n' ||
                hit[ -1 ] == '"';
            int after_pos = ( int ) ( hit - line + token_len );
            int after_ok = ( ( size_t ) after_pos >= strlen( line ) ) ||
                isspace( ( unsigned char ) line[ after_pos ] ) ||
                line[ after_pos ] == '\n' ||
                line[ after_pos ] == '"';

            if ( before_ok && after_ok ) {
                found = 1;
                break;
            }
            cursor = ( char * ) hit + 1;
        }

        if ( found ) {
            break;
        }
    }

    if ( fclose( fp ) != 0 ) {
        return found;
    }

    return found;
}

static int
has_boot_autoload_marker( void )
{
    if ( file_exists( kModulesLoadPath ) && file_contains_word( kModulesLoadPath, "myass" ) ) {
        return 1;
    }

    if ( file_exists( kOpenrcModulesPath ) && file_contains_word( kOpenrcModulesPath, "myass" ) ) {
        return 1;
    }

    if ( file_exists( "/etc/modules" ) && file_contains_word( "/etc/modules", "myass" ) ) {
        return 1;
    }

    return 0;
}

static int
find_candidate_module_paths(
    const char *kernel_release,
    char *out,
    size_t out_size
)
{
    const char *patterns[] = {
        "/lib/modules/%s/updates/dkms/%s",
        "/lib/modules/%s/extra/%s",
        "/lib/modules/%s/kernel/drivers/myass/%s",
        "/lib/modules/%s/kernel/drivers/misc/myass/%s",
        NULL
    };
    char path[ PATH_MAX ];
    size_t i;

    if ( !kernel_release || !out || out_size == 0 ) {
        return 0;
    }

    for ( i = 0; patterns[ i ]; i++ ) {
        snprintf(
            path,
            sizeof( path ),
            patterns[ i ],
            kernel_release,
            kModuleName
        );
        if ( file_exists( path ) ) {
            snprintf( out, out_size, "%s", path );
            return 1;
        }
    }

    return 0;
}

static int
enable_openrc_boot_module( void )
{
    char cmd[ SHELL_CMD_MAX ];

    if ( !file_exists( kOpenrcModulesPath ) ) {
        return 0;
    }

    if ( !command_exists( "grep" ) ) {
        fprintf(
            stderr,
            "Cannot configure OpenRC autoload: grep command unavailable.\n"
        );
        return 0;
    }

    if ( command_exists( "sed" ) ) {
        snprintf(
            cmd,
            sizeof( cmd ),
            "if grep -Eq '(^|[[:space:]])myass([[:space:]]|$)' '%s'; then\n"
            "  exit 0\n"
            "fi\n"
            "if grep -Eq '^[[:space:]]*modules[[:space:]]*=' '%s'; then\n"
            "  sed -i -E 's/^([[:space:]]*modules[[:space:]]*=)\"([^\"]*)\"$/\\1\"\\2 myass\"/; "
            "t; s/^([[:space:]]*modules[[:space:]]*=)(.*)$/\\1\"\\2 myass\"/' '%s'\n"
            "else\n"
            "  printf '\\nmodules=\"myass\"\\n' >> '%s'\n"
            "fi\n"
            "grep -Eq '(^|[[:space:]])myass([[:space:]]|$)' '%s'",
            kOpenrcModulesPath,
            kOpenrcModulesPath,
            kOpenrcModulesPath,
            kOpenrcModulesPath,
            kOpenrcModulesPath
        );
        if ( run_shell_capture_stderr( cmd ) ) {
            fprintf(
                stderr,
                "Registered myass in OpenRC module list: %s\n",
                kOpenrcModulesPath
            );
            return 1;
        }
    }

    return 0;
}

static int
enable_boot_module_autoload( void )
{
    char cmd[ SHELL_CMD_MAX ];

    if ( !command_exists( "mkdir" ) || !command_exists( "sh" ) ) {
        fprintf(
            stderr,
            "Cannot configure boot autoload because mkdir/sh are unavailable.\n"
        );
        return 0;
    }

    if ( !file_exists( "/etc/modules-load.d" ) ) {
        snprintf( cmd, sizeof( cmd ), "mkdir -p /etc/modules-load.d" );
        if ( !run_shell( cmd ) ) {
            fprintf(
                stderr,
                "Could not create /etc/modules-load.d; trying /etc/modules fallback.\n"
            );
        }
    }

    if ( write_text_file( kModulesLoadPath, "myass\n" ) ) {
        if ( command_exists( "systemctl" ) ) {
            run_shell( "systemctl daemon-reload >/dev/null 2>&1" );
            if ( !run_shell_capture_stderr(
                    "systemctl restart systemd-modules-load.service >/dev/null 2>&1" ) ) {
                fprintf(
                    stderr,
                    "Could not restart systemd-modules-load now; autoload will apply on next boot.\n"
                );
            }
        }
        return 1;
    }

    if ( enable_openrc_boot_module() ) {
        return 1;
    }

    if ( file_exists( "/etc/modules" ) ) {
        snprintf(
            cmd,
            sizeof( cmd ),
            "grep -qxF \"myass\" /etc/modules || echo \"myass\" >> /etc/modules"
        );
        if ( run_shell_capture_stderr( cmd ) ) {
            fprintf(
                stderr,
                "Registered myass in /etc/modules fallback for boot-time loading.\n"
            );
            return 1;
        }
    }

    fprintf(
        stderr,
        "Could not persist boot-time autoload registration for myass. "
        "Kernel module may not load automatically on next boot.\n"
    );
    return 0;
}

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
        "%s - Not My ASS - Have you ever wanted to feel like a BSODkid on Linux? (%s)\n"
        "Usage:\n"
        "  %s [options]\n"
        "\n"
        "Options:\n"
        "  -h, --help, /?              Show this help text and exit.\n"
        "  -crash, --crash, -driver,\n"
        "  /driver, --driver            Trigger driver crash immediately.\n"
        "                               You can add a custom reason using:\n"
        "                               --reason <text>\n"
        "  -sysrq, --sysrq              Trigger kernel crash via SysRq 'c'.\n"
        "  -kill-init, --kill-init      Trigger SIGKILL 1.\n"
        "  --delay <seconds>            Delay before executing the selected action.\n"
        "  --dry-run                    Print commands without executing mutations.\n"
        "  --log <path>                 Mirror status output to a log file.\n"
        "  -install-driver,\n"
        "  /install-driver,\n"
        "  --install-driver[=mode]      Persistently install module. Optional mode:\n"
        "                               --install-driver once|boot (default boot).\n"
        "  --uninstall-driver            Remove installed module and startup wiring.\n"
        "  --status                     Print machine-readable MYASS_STATUS_* lines.\n"
        "  --version                    Print version and exit.\n"
        "\n",
        exe_name,
        kDkmsModuleVersion,
        exe_name
    );
}

static void
print_banner( void )
{
    fprintf(
        stderr,
        "Not My ASS - Have you ever wanted to feel like a BSODkid on Linux? (%s)\n",
        kDkmsModuleVersion
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

    if ( g_dry_run ) {
        fprintf(
            stderr,
            "INSTALL MYASS [dry-run] auto-confirmed.\n"
        );
        return 1;
    }

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

    if ( strcmp( filename, "myass.c" ) == 0 ) {
        snprintf( out, out_size, "%s/../sys/%s", exe_dir, filename );
        if ( file_exists( out ) ) {
            return 1;
        }
    }

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
install_with_dkms( int enable_boot_load )
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
    if ( !run_shell_or_dry( cmd ) ) {
        fprintf( stderr, "Could not clear previous DKMS source directory %s.\n", dkms_src_root );
        return 0;
    }

    snprintf( cmd, sizeof( cmd ), "mkdir -p \"%s\"", dkms_src_root );
    if ( !run_shell_or_dry( cmd ) ) {
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
    run_shell_or_dry( cmd );

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
    if ( !run_shell_capture_stderr_or_dry( cmd ) ) {
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
    if ( !run_shell_capture_stderr_or_dry( cmd ) ) {
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
    if ( !run_shell_capture_stderr_or_dry( cmd ) ) {
        cleanup_dkms_workspace( dkms_src_root );
        return 0;
    }

    if ( command_exists( "modprobe" ) ) {
        if ( run_shell_capture_stderr_or_dry( "modprobe myass" ) ) {
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
    if ( installed && enable_boot_load ) {
        if ( !enable_boot_module_autoload() ) {
            fprintf(
                stderr,
                "DKMS install succeeded, but automatic load-on-boot could not be configured.\n"
            );
        }
    }
    cleanup_dkms_workspace( dkms_src_root );
    return installed;
}

static int
install_driver_permanently( int enable_boot_load )
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
        if ( install_with_dkms( enable_boot_load ) ) {
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
    if ( !run_shell_or_dry( cmd ) ) {
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
    if ( !run_shell_or_dry( cmd ) ) {
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
        run_shell_or_dry( cmd );
    }

    if ( command_exists( "modprobe" ) ) {
        if ( run_shell_capture_stderr_or_dry( "modprobe myass" ) ) {
            if ( enable_boot_load && !enable_boot_module_autoload() ) {
                fprintf(
                    stderr,
                    "Module is loaded, but automatic load-on-boot could not be configured.\n"
                );
            }
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
    if ( run_shell_capture_stderr_or_dry( cmd ) ) {
        if ( enable_boot_load && !enable_boot_module_autoload() ) {
            fprintf(
                stderr,
                "Module is loaded, but automatic load-on-boot could not be configured.\n"
            );
        }
        return 1;
    }

    return 0;
}

static int
uninstall_boot_registration( void )
{
    char cmd[ SHELL_CMD_MAX ];
    int changed = 0;

    if ( file_exists( kModulesLoadPath ) ) {
        snprintf(
            cmd,
            sizeof( cmd ),
            "rm -f \"%s\"",
            kModulesLoadPath
        );
        run_shell_or_dry( cmd );
        changed = 1;
    }

    if ( file_exists( kOpenrcModulesPath ) ) {
        snprintf(
            cmd,
            sizeof( cmd ),
            "sed -i -E 's/(^|[[:space:]])myass([[:space:]]|$)/ /g; s/[[:space:]]{2,}/ /g; s/^([[:space:]]*modules[[:space:]]*=[[:space:]]*\"[[:space:]]*)[[:space:]]*\"$/\\1\"/g' \"%s\"",
            kOpenrcModulesPath
        );
        run_shell_or_dry( cmd );
        changed = 1;
    }

    if ( file_exists( "/etc/modules" ) ) {
        snprintf(
            cmd,
            sizeof( cmd ),
            "grep -v '^myass$' < /etc/modules > /tmp/myass-modules.$$ && mv /tmp/myass-modules.$$ /etc/modules"
        );
        run_shell_or_dry( cmd );
        changed = 1;
    }

    return changed;
}

static int
unload_loaded_module( void )
{
    if ( !module_is_loaded() ) {
        return 1;
    }

    if ( command_exists( "modprobe" ) ) {
        if ( run_shell_capture_stderr_or_dry( "modprobe -r myass" ) ) {
            return 1;
        }
        fprintf( stderr, "modprobe -r failed, attempting rmmod...\n" );
    }

    return run_shell_capture_stderr_or_dry( "rmmod myass" );
}

static int
uninstall_driver( void )
{
    struct utsname uts;
    char installed_module[ PATH_MAX ];
    int had_any = 0;

    if ( geteuid() != 0 ) {
        fprintf( stderr, "Persistent uninstall requires root privileges.\n" );
        return 0;
    }

    if ( !confirm_action( "uninstalling myass module" ) ) {
        return 0;
    }

    if ( uname( &uts ) != 0 ) {
        fprintf( stderr, "Could not read kernel release.\n" );
        return 0;
    }

    if ( find_candidate_module_paths( uts.release, installed_module, sizeof( installed_module ) ) ) {
        had_any = 1;
        if ( !run_shell_capture_stderr_or_dry( "sync" ) ) {
            /* ignore */
        }
        snprintf(
            installed_module,
            sizeof( installed_module ),
            "rm -f \"%s\"",
            installed_module
        );
        run_shell_or_dry( installed_module );
        if ( command_exists( "depmod" ) ) {
            char depmod_cmd[ SHELL_CMD_MAX ];
            snprintf(
                depmod_cmd,
                sizeof( depmod_cmd ),
                "depmod -a %s",
                uts.release
            );
            run_shell_or_dry( depmod_cmd );
        }
    }

    if ( command_exists( "dkms" ) ) {
        char dkms_cmd[ SHELL_CMD_MAX ];
        snprintf(
            dkms_cmd,
            sizeof( dkms_cmd ),
            "dkms remove -m %s -v %s --all",
            kDkmsModuleName,
            kDkmsModuleVersion
        );
        if ( run_shell_capture_stderr_or_dry( dkms_cmd ) ) {
            had_any = 1;
        }
    }

    if ( unload_loaded_module() ) {
        had_any = 1;
    }

    if ( uninstall_boot_registration() ) {
        had_any = 1;
    }

    if ( !had_any ) {
        fprintf( stderr, "No myass module installation artifacts were found.\n" );
    }

    return had_any;
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
    rect_t driver = {
        GUI_BTN_DRIVER_X, GUI_BTN_DRIVER_Y,
        GUI_BTN_DRIVER_W, GUI_BTN_DRIVER_H
    };
    rect_t sysrq = {
        GUI_BTN_SYSRQ_X, GUI_BTN_SYSRQ_Y,
        GUI_BTN_SYSRQ_W, GUI_BTN_SYSRQ_H
    };
    rect_t kill_init = {
        GUI_BTN_KILL_X, GUI_BTN_KILL_Y,
        GUI_BTN_KILL_W, GUI_BTN_KILL_H
    };
    rect_t exit = {
        GUI_BTN_EXIT_X, GUI_BTN_EXIT_Y,
        GUI_BTN_EXIT_W, GUI_BTN_EXIT_H
    };

    XClearWindow( display, window );
    draw_button( display, window, gc, driver, "Driver crash" );
    draw_button( display, window, gc, sysrq, "Sysrq 'c' crash" );
    draw_button( display, window, gc, kill_init, "Kill init (SIGKILL 1)" );
    draw_button( display, window, gc, exit, "Exit" );
    XDrawString( display, window, gc, 5, 15, "Choose your poison", 17 );
}
#endif

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
        if ( install_driver_permanently( g_install_mode == INSTALL_MODE_BOOT ) ) {
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
        if ( run_shell_capture_stderr_or_dry( "modprobe myass" ) ) {
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
        char path[ PATH_MAX ];

        if ( uname( &uts ) == 0 ) {
            if ( find_candidate_module_paths( uts.release, path, sizeof( path ) ) ) {
                if ( insmod_module( path ) ) {
                    return access( kDriverPath, F_OK ) == 0;
                }
            }
        }
    }

    if ( insmod_module( "myass.ko" ) ) {
        return access( kDriverPath, F_OK ) == 0;
    }

    fprintf( stderr, "Could not auto-load myass module. Run as root or install module manually.\n" );
    return 0;
}

static int
trigger_driver_crash_with_reason( const char *reason )
{
    int fd;
    const char *payload = reason && reason[ 0 ] ? reason : kDefaultCrashReason;
    size_t payload_len;

    payload_len = strlen( payload );
    fprintf( stderr, "Driver crash requested. reason: %s\n", payload );

    fd = open( kDriverPath, O_WRONLY );
    if ( fd >= 0 ) {
        if ( write( fd, payload, payload_len ) != ( ssize_t ) payload_len ) {
            fprintf( stderr, "Failed to send custom reason to driver: %s\n", strerror( errno ) );
            close( fd );
            raise( SIGSEGV );
            return 0;
        }
        close( fd );
        return 1;
    }

    fprintf(
        stderr,
        "Could not open driver %s: %s\n",
        kDriverPath,
        strerror( errno )
    );
    raise( SIGSEGV );
    return 0;
}

static int
trigger_sysrq_crash( void )
{
    int fd;

    fd = open( kSysrqPath, O_WRONLY );
    if ( fd < 0 ) {
        fprintf(
            stderr,
            "Could not open %s: %s\n",
            kSysrqPath,
            strerror( errno )
        );
        return 0;
    }

    if ( write( fd, "c", 1 ) != 1 ) {
        fprintf( stderr, "Failed to trigger SysRq c: %s\n", strerror( errno ) );
        close( fd );
        return 0;
    }

    close( fd );
    return 1;
}

static int
trigger_kill_init( void )
{
    if ( kill( 1, SIGKILL ) != 0 ) {
        fprintf( stderr, "Could not SIGKILL pid 1: %s\n", strerror( errno ) );
        return 0;
    }
    return 1;
}

static int
execute_poison(
    poison_t poison,
    const char *reason,
    int ensure_driver,
    int interactive_confirmation,
    int always_delay
    )
{
    if ( always_delay ) {
        if ( !apply_delay() ) {
            return 0;
        }
    }

    if ( poison == POISON_DRIVER ) {
        if ( !ensure_driver_loaded( ensure_driver ) ) {
            fprintf(
                stderr,
                "Driver not available; continuing to crash attempt.\n"
            );
            return 0;
        }
        if ( interactive_confirmation && !has_tty() ) {
            fprintf(
                stderr,
                "Driver crash was requested non-interactively. Using preselected reason if provided.\n"
            );
        }
        trigger_driver_crash_with_reason( reason );
        return 1;
    }

    if ( poison == POISON_SYSRQ ) {
        if ( interactive_confirmation && !confirm_action( "kernel SysRq crash trigger" ) ) {
            return 0;
        }
        return trigger_sysrq_crash();
    }

    if ( poison == POISON_KILL_INIT ) {
        if ( interactive_confirmation && !confirm_action( "kill init (SIGKILL 1)" ) ) {
            return 0;
        }
        return trigger_kill_init();
    }

    return 0;
}

int
main( int argc, char **argv )
{
	int i;
	int wants_cli = 0;
	int show_help = 0;
	int install_driver = 0;
	int parsed_delay = 0;
	int rc = 0;
	poison_t poison = POISON_DRIVER;
	const char *log_path = NULL;
	const char *reason_arg = NULL;
	char reason[ CRASH_REASON_MAX + 1 ];
	char *delay_arg;
	char *eq;
	install_mode_t parsed_mode;
	reason[ 0 ] = '\0';

	print_banner();

	for ( i = 1; i < argc; i++ ) {
		if ( strcmp( argv[ i ], "/crash" ) == 0 ||
			 strcmp( argv[ i ], "-crash" ) == 0 ||
			 strcmp( argv[ i ], "--crash" ) == 0 ||
			 strcmp( argv[ i ], "/driver" ) == 0 ||
			 strcmp( argv[ i ], "-driver" ) == 0 ||
			 strcmp( argv[ i ], "--driver" ) == 0 ) {
			wants_cli = 1;
			poison = POISON_DRIVER;
			continue;
		}

		if ( strcmp( argv[ i ], "/sysrq" ) == 0 ||
			 strcmp( argv[ i ], "-sysrq" ) == 0 ||
			 strcmp( argv[ i ], "--sysrq" ) == 0 ) {
			wants_cli = 1;
			poison = POISON_SYSRQ;
			continue;
		}

		if ( strcmp( argv[ i ], "/kill-init" ) == 0 ||
			 strcmp( argv[ i ], "-kill-init" ) == 0 ||
			 strcmp( argv[ i ], "--kill-init" ) == 0 ) {
			wants_cli = 1;
			poison = POISON_KILL_INIT;
			continue;
		}

		if ( strcmp( argv[ i ], "--reason" ) == 0 || strcmp( argv[ i ], "-r" ) == 0 ) {
			if ( i + 1 >= argc ) {
				fprintf( stderr, "Missing reason value after %s\n", argv[ i ] );
				return 1;
			}
			reason_arg = argv[ ++i ];
			continue;
		}

		if ( strncmp( argv[ i ], "--delay=", 8 ) == 0 ) {
			delay_arg = argv[ i ] + 8;
			if ( *delay_arg == '\0' ) {
				fprintf( stderr, "Missing delay seconds for --delay=<seconds>.\n" );
				return 1;
			}
			g_delay_seconds = atoi( delay_arg );
			parsed_delay = 1;
			continue;
		}

		if ( strcmp( argv[ i ], "--delay" ) == 0 ) {
			if ( i + 1 >= argc ) {
				fprintf( stderr, "Missing seconds for --delay.\n" );
				return 1;
			}
			g_delay_seconds = atoi( argv[ ++i ] );
			parsed_delay = 1;
			continue;
		}

		if ( strcmp( argv[ i ], "--dry-run" ) == 0 ) {
			g_dry_run = 1;
			continue;
		}

		if ( strncmp( argv[ i ], "--log=", 6 ) == 0 ) {
			log_path = argv[ i ] + 6;
			if ( log_path[ 0 ] == '\0' ) {
				fprintf( stderr, "Missing path for --log=<path>.\n" );
				return 1;
			}
			continue;
		}
		if ( strcmp( argv[ i ], "--log" ) == 0 ) {
			if ( i + 1 >= argc ) {
				fprintf( stderr, "Missing path for --log.\n" );
				return 1;
			}
			log_path = argv[ ++i ];
			continue;
		}

		if ( strcmp( argv[ i ], "--status" ) == 0 ) {
			g_status_requested = 1;
			continue;
		}

		if ( strcmp( argv[ i ], "--version" ) == 0 ) {
			g_version_requested = 1;
			continue;
		}

		if ( strncmp( argv[ i ], "--install-driver", 16 ) == 0 ) {
			eq = argv[ i ] + 16;
			install_driver = 1;
			if ( eq[ 0 ] == '\0' ) {
				continue;
			}
			if ( eq[ 0 ] == '=' ) {
				if ( !parse_install_mode( eq + 1, &parsed_mode ) ) {
					fprintf(
						stderr,
						"Invalid mode for --install-driver=%s. Use once or boot.\n",
						eq + 1
					);
					return 1;
				}
				g_install_mode = parsed_mode;
				continue;
			}
			if ( i + 1 < argc && parse_install_mode( argv[ i + 1 ], &parsed_mode ) ) {
				g_install_mode = parsed_mode;
				i++;
				continue;
			}
			continue;
		}

		if ( strncmp( argv[ i ], "--install-mode=", 15 ) == 0 ) {
			if ( !parse_install_mode( argv[ i ] + 15, &parsed_mode ) ) {
				fprintf(
					stderr,
					"Invalid mode for --install-mode=%s. Use once or boot.\n",
					argv[ i ] + 15
				);
				return 1;
			}
			g_install_mode = parsed_mode;
			continue;
		}

		if ( strcmp( argv[ i ], "--install-mode" ) == 0 ) {
			if ( i + 1 >= argc ) {
				fprintf(
					stderr,
					"Missing mode for --install-mode (once|boot).\n"
				);
				return 1;
			}
			if ( !parse_install_mode( argv[ i + 1 ], &parsed_mode ) ) {
				fprintf(
					stderr,
					"Invalid mode for --install-mode. Use once or boot.\n"
				);
				return 1;
			}
			g_install_mode = parsed_mode;
			i++;
			continue;
		}

		if ( strcmp( argv[ i ], "--uninstall-driver" ) == 0 ) {
			g_uninstall_requested = 1;
			continue;
		}

		if ( strcmp( argv[ i ], "-h" ) == 0 ||
			 strcmp( argv[ i ], "--help" ) == 0 ||
			 strcmp( argv[ i ], "/?" ) == 0 ) {
			show_help = 1;
			continue;
		}

		if (
			strcmp( argv[ i ], "/install-driver" ) == 0 ||
			strcmp( argv[ i ], "-install-driver" ) == 0
		) {
			install_driver = 1;
			continue;
		}

		if ( argv[ i ][ 0 ] == '-' ) {
			fprintf( stderr, "Unrecognized option: %s\n", argv[ i ] );
			fprintf( stderr, "Run --help for usage.\n" );
			return 1;
		}
	}

	if ( show_help ) {
		print_usage( argv[ 0 ] );
		return 0;
	}

	if ( parsed_delay && g_delay_seconds < 0 ) {
		fprintf( stderr, "Delay must be a non-negative integer.\n" );
		return 1;
	}

	if ( log_path && !open_log_file( log_path ) ) {
		return 1;
	}

	if ( g_version_requested ) {
		status_kv( "VERSION", kDkmsModuleVersion );
		status_kv( "MODULE_NAME", kDkmsModuleName );
		close_log_file();
		return 0;
	}

	if ( g_status_requested ) {
		status_print_mode();
		close_log_file();
		return 0;
	}

	if ( g_uninstall_requested ) {
		if ( install_driver ) {
			fprintf(
				stderr,
				"Conflicting request: both --install-driver and --uninstall-driver were provided.\n"
			);
			close_log_file();
			return 1;
		}
		rc = uninstall_driver();
		close_log_file();
		return rc ? 0 : 1;
	}

	if ( wants_cli ) {
		if ( reason_arg && reason_arg[ 0 ] ) {
			snprintf( reason, sizeof( reason ), "%s", reason_arg );
		}
		if ( !execute_poison(
			poison,
			reason,
			install_driver,
			1,
			1
		) ) {
			close_log_file();
			return 1;
		}
		close_log_file();
		return 0;
	}

	if ( install_driver ) {
		if ( !ensure_driver_loaded( 1 ) ) {
			fprintf( stderr, "Driver install requested; installation did not complete.\n" );
			close_log_file();
			return 1;
		}
		close_log_file();
		return 0;
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
		rect_t driver_btn = {
			GUI_BTN_DRIVER_X, GUI_BTN_DRIVER_Y,
			GUI_BTN_DRIVER_W, GUI_BTN_DRIVER_H
		};
		rect_t sysrq_btn = {
			GUI_BTN_SYSRQ_X, GUI_BTN_SYSRQ_Y,
			GUI_BTN_SYSRQ_W, GUI_BTN_SYSRQ_H
		};
		rect_t kill_btn = {
			GUI_BTN_KILL_X, GUI_BTN_KILL_Y,
			GUI_BTN_KILL_W, GUI_BTN_KILL_H
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
				220,
				220,
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
							driver_btn ) ) {
						char local_reason[ CRASH_REASON_MAX + 1 ];
						read_reason_from_stdin( local_reason, sizeof( local_reason ) );
						execute_poison(
							POISON_DRIVER,
							local_reason,
							install_driver,
							0,
							1
						);
						continue;
					}
					if ( point_in_rect(
							event.xbutton.x,
							event.xbutton.y,
							sysrq_btn ) ) {
						execute_poison(
							POISON_SYSRQ,
							NULL,
							0,
							0,
							1
						);
						continue;
					}
					if ( point_in_rect(
							event.xbutton.x,
							event.xbutton.y,
							kill_btn ) ) {
						execute_poison(
							POISON_KILL_INIT,
							NULL,
							0,
							0,
							1
						);
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
	close_log_file();
	return 0;
}

#endif
