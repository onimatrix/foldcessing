/*
 * Foldcessing - A preprocessor for Processing sketches with subdirectory support
 *
 * Copyright (C) 2025 Foldcessing Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <ctype.h>

// Declare missing Windows functions for TCC
#ifndef ATTACH_PARENT_PROCESS
#define ATTACH_PARENT_PROCESS ((DWORD)-1)
#endif

typedef BOOL (WINAPI *AttachConsoleFunc)(DWORD dwProcessId);
typedef BOOL (WINAPI *FreeConsoleFunc)(VOID);

#define MAX_PATH_LEN 4096
#define MAX_FILES 10000
#define MAX_LINE 8192
#define MAX_IGNORE_PATTERNS 100

typedef struct {
    char path[MAX_PATH_LEN];
    char relative[MAX_PATH_LEN];
} FileEntry;

typedef struct {
    int start_line;
    int end_line;
    char relative[MAX_PATH_LEN];
} LineMapping;

typedef struct {
    char processing_path[MAX_PATH_LEN];
    char ignore_patterns[MAX_IGNORE_PATTERNS][MAX_PATH_LEN];
    int ignore_count;
    char default_action[256];
} Config;

FileEntry files[MAX_FILES];
LineMapping line_map[MAX_FILES];
int file_count = 0;
int total_lines = 0;  // Total lines in concatenated output.pde
Config config = {0};

// Case-insensitive string comparison for sorting
int strcasecmp_win(const char *s1, const char *s2) {
    return _stricmp(s1, s2);
}

// Trim whitespace from string
void trim(char *str) {
    char *start = str;
    while (isspace(*start)) start++;
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
    char *end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) {
        *end = '\0';
        end--;
    }
}

// Simple wildcard matching (* and ?)
int wildcard_match(const char *pattern, const char *str) {
    while (*pattern && *str) {
        if (*pattern == '*') {
            pattern++;
            if (!*pattern) return 1;
            while (*str) {
                if (wildcard_match(pattern, str)) return 1;
                str++;
            }
            return 0;
        } else if (*pattern == '?' || tolower(*pattern) == tolower(*str)) {
            pattern++;
            str++;
        } else {
            return 0;
        }
    }
    while (*pattern == '*') pattern++;
    return !*pattern && !*str;
}

// Check if a path should be ignored
int should_ignore(const char *relative_path) {
    for (int i = 0; i < config.ignore_count; i++) {
        if (wildcard_match(config.ignore_patterns[i], relative_path)) {
            return 1;
        }
    }
    return 0;
}

// Parse config file
void parse_config(const char *profile) {
    FILE *f = fopen(".foldcessing", "r");
    if (!f) return;

    char line[MAX_LINE];
    char current_section[256] = "general";
    int in_target_section = 1;
    char target_section[256];

    if (profile) {
        snprintf(target_section, sizeof(target_section), "profile:%s", profile);
    } else {
        strcpy(target_section, "general");
    }

    while (fgets(line, sizeof(line), f)) {
        trim(line);

        // Skip empty lines and comments
        if (!line[0] || line[0] == '#' || line[0] == ';') continue;

        // Check for section header
        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end) {
                *end = '\0';
                strcpy(current_section, line + 1);
                in_target_section = (strcasecmp_win(current_section, "general") == 0 ||
                                    strcasecmp_win(current_section, target_section) == 0);
            }
            continue;
        }

        if (!in_target_section) continue;

        // Parse key=value
        char *equals = strchr(line, '=');
        if (!equals) continue;

        *equals = '\0';
        char *key = line;
        char *value = equals + 1;
        trim(key);
        trim(value);

        if (strcasecmp_win(key, "processing_path") == 0) {
            // Profile values override general
            if (strcasecmp_win(current_section, target_section) == 0 || !config.processing_path[0]) {
                strncpy(config.processing_path, value, MAX_PATH_LEN - 1);
            }
        } else if (strcasecmp_win(key, "ignore") == 0) {
            // Parse comma-separated ignore patterns
            char *token = strtok(value, ",");
            while (token && config.ignore_count < MAX_IGNORE_PATTERNS) {
                trim(token);
                if (token[0]) {
                    strncpy(config.ignore_patterns[config.ignore_count++], token, MAX_PATH_LEN - 1);
                }
                token = strtok(NULL, ",");
            }
        } else if (strcasecmp_win(key, "default_action") == 0) {
            // Profile values override general
            if (strcasecmp_win(current_section, target_section) == 0 || !config.default_action[0]) {
                strncpy(config.default_action, value, sizeof(config.default_action) - 1);
            }
        }
    }

    fclose(f);
}

// Check if string ends with suffix
int ends_with(const char *str, const char *suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return 0;
    return strcasecmp_win(str + str_len - suffix_len, suffix) == 0;
}

// Recursively collect .pde files
void collect_files(const char *dir_path, const char *relative_path) {
    WIN32_FIND_DATA find_data;
    HANDLE hFind;
    char search_path[MAX_PATH_LEN];

    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

    hFind = FindFirstFile(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) return;

    // Collect directories and files separately
    char **directories = malloc(sizeof(char*) * 1000);
    char **dir_relatives = malloc(sizeof(char*) * 1000);
    char **pde_files = malloc(sizeof(char*) * 1000);
    char **pde_relatives = malloc(sizeof(char*) * 1000);
    int dir_count = 0, pde_count = 0;

    do {
        if (strcmp(find_data.cFileName, ".") == 0 ||
            strcmp(find_data.cFileName, "..") == 0) continue;

        // Skip output directory
        if (strcasecmp_win(find_data.cFileName, "output") == 0 &&
            (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;

        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName);

        char new_relative[MAX_PATH_LEN];
        if (strlen(relative_path) == 0) {
            snprintf(new_relative, sizeof(new_relative), "%s", find_data.cFileName);
        } else {
            snprintf(new_relative, sizeof(new_relative), "%s/%s", relative_path, find_data.cFileName);
        }

        // Check if this path should be ignored
        if (should_ignore(new_relative)) continue;

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            directories[dir_count] = _strdup(full_path);
            dir_relatives[dir_count] = _strdup(new_relative);
            dir_count++;
        } else if (ends_with(find_data.cFileName, ".pde")) {
            pde_files[pde_count] = _strdup(full_path);
            pde_relatives[pde_count] = _strdup(new_relative);
            pde_count++;
        }
    } while (FindNextFile(hFind, &find_data));

    FindClose(hFind);

    // Sort directories alphabetically
    for (int i = 0; i < dir_count - 1; i++) {
        for (int j = i + 1; j < dir_count; j++) {
            char *name_i = strrchr(directories[i], '\\');
            char *name_j = strrchr(directories[j], '\\');
            if (name_i && name_j && strcasecmp_win(name_i + 1, name_j + 1) > 0) {
                char *temp = directories[i];
                directories[i] = directories[j];
                directories[j] = temp;
                temp = dir_relatives[i];
                dir_relatives[i] = dir_relatives[j];
                dir_relatives[j] = temp;
            }
        }
    }

    // Sort pde files alphabetically
    for (int i = 0; i < pde_count - 1; i++) {
        for (int j = i + 1; j < pde_count; j++) {
            char *name_i = strrchr(pde_files[i], '\\');
            char *name_j = strrchr(pde_files[j], '\\');
            if (name_i && name_j && strcasecmp_win(name_i + 1, name_j + 1) > 0) {
                char *temp = pde_files[i];
                pde_files[i] = pde_files[j];
                pde_files[j] = temp;
                temp = pde_relatives[i];
                pde_relatives[i] = pde_relatives[j];
                pde_relatives[j] = temp;
            }
        }
    }

    // Recursively process subdirectories (depth-first)
    for (int i = 0; i < dir_count; i++) {
        collect_files(directories[i], dir_relatives[i]);
        free(directories[i]);
        free(dir_relatives[i]);
    }

    // Add .pde files from current directory
    for (int i = 0; i < pde_count; i++) {
        if (file_count < MAX_FILES) {
            strcpy(files[file_count].path, pde_files[i]);
            strcpy(files[file_count].relative, pde_relatives[i]);
            file_count++;
        }
        free(pde_files[i]);
        free(pde_relatives[i]);
    }

    free(directories);
    free(dir_relatives);
    free(pde_files);
    free(pde_relatives);
}

// Translate line number from output.pde to original source file
void translate_line(int line_num, char *output, size_t output_size) {
    // Java class file format stores line numbers as 16-bit unsigned (0-65535)
    // If output.pde exceeds 65536 lines, reported line is actual_line % 65536
    // Try all possible wrapped values: line_num, line_num+65536, line_num+131072, etc.

    #define LINE_WRAP 65536

    typedef struct {
        int file_index;
        int actual_line;
        int original_line;
    } MatchCandidate;

    MatchCandidate candidates[10];  // Store up to 10 possible matches
    int candidate_count = 0;

    // Try wrapped line numbers until we exceed total_lines
    for (int k = 0; k * LINE_WRAP < total_lines && k < 10; k++) {
        int candidate_line = line_num + (k * LINE_WRAP);
        if (candidate_line > total_lines) break;

        // Check if this candidate falls within any file's range
        for (int i = 0; i < file_count; i++) {
            if (candidate_line >= line_map[i].start_line &&
                candidate_line <= line_map[i].end_line) {
                candidates[candidate_count].file_index = i;
                candidates[candidate_count].actual_line = candidate_line;
                candidates[candidate_count].original_line = candidate_line - line_map[i].start_line + 1;
                candidate_count++;
                break;  // Found match for this k, move to next
            }
        }
    }

    // Report based on number of matches
    if (candidate_count == 0) {
        // No match found - use literal line number
        snprintf(output, output_size, "output.pde:%d", line_num);
    } else if (candidate_count == 1) {
        // Single match - report confidently
        int i = candidates[0].file_index;
        snprintf(output, output_size, "%s:%d",
                 line_map[i].relative, candidates[0].original_line);
    } else {
        // Multiple matches - report all possibilities
        char temp[MAX_LINE] = {0};
        for (int j = 0; j < candidate_count; j++) {
            int i = candidates[j].file_index;
            char part[512];
            snprintf(part, sizeof(part), "%s%s:%d",
                     (j > 0) ? " or " : "",
                     line_map[i].relative,
                     candidates[j].original_line);
            strncat(temp, part, sizeof(temp) - strlen(temp) - 1);
        }
        strncat(temp, " (line wrapping)", sizeof(temp) - strlen(temp) - 1);
        snprintf(output, output_size, "%s", temp);
    }
}

// Process and translate a line of output from processing-java
void process_output_line(const char *line) {
    char buffer[MAX_LINE];
    strcpy(buffer, line);

    // Look for patterns like "output.pde:36" or "output.pde:36:4:36:4"
    char *ptr = buffer;
    while ((ptr = strstr(ptr, "output.pde:")) != NULL) {
        ptr += 11; // Skip "output.pde:"

        char *end_ptr;
        int line_num = strtol(ptr, &end_ptr, 10);

        if (end_ptr != ptr && line_num > 0) {
            // Found a line number, translate it
            char translated[MAX_PATH_LEN];
            translate_line(line_num, translated, sizeof(translated));

            // Parse optional column number
            int col_num = -1;
            if (*end_ptr == ':') {
                char *col_end;
                col_num = strtol(end_ptr + 1, &col_end, 10);
                if (col_end != end_ptr + 1) {
                    end_ptr = col_end;
                }
            }

            // Skip any remaining :number:number patterns (redundant position info)
            while (*end_ptr == ':') {
                char *skip_end;
                strtol(end_ptr + 1, &skip_end, 10);
                if (skip_end == end_ptr + 1) break; // Not a number
                end_ptr = skip_end;
            }

            // Print everything before "output.pde:"
            char *start = strstr(buffer, "output.pde:");
            *start = '\0';
            printf("%s%s", buffer, translated);
            if (col_num >= 0) {
                printf(":%d", col_num);
            }
            printf("%s", end_ptr);
            return;
        }
    }

    // No translation needed, print as-is
    printf("%s", line);
}

int main(int argc, char *argv[]) {
    // Parse arguments for --profile
    char *profile = NULL;
    int first_processing_arg = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--profile") == 0 && i + 1 < argc) {
            profile = argv[i + 1];
            first_processing_arg = i + 2;
            break;
        }
    }

    // Load config file
    parse_config(profile);

    // Detect if running without console (double-clicked)
    // Try to attach to parent's console. If we can, we were launched from a console.
    // Load functions dynamically for TCC compatibility
    HMODULE kernel32 = GetModuleHandle("kernel32.dll");
    FreeConsoleFunc pFreeConsole = (FreeConsoleFunc)GetProcAddress(kernel32, "FreeConsole");
    AttachConsoleFunc pAttachConsole = (AttachConsoleFunc)GetProcAddress(kernel32, "AttachConsole");

    int has_console = 0;  // Assume no console (double-clicked)
    if (pFreeConsole && pAttachConsole) {
        pFreeConsole();  // Detach from current console if any
        BOOL attached = pAttachConsole(ATTACH_PARENT_PROCESS);
        has_console = attached;  // True if we successfully attached to parent console
    }

    // Pre-validate: if we'll need processing-java, check it exists BEFORE folding
    int will_need_processing = (argc > first_processing_arg) ||
                               (config.processing_path[0] && config.default_action[0]);

    if (will_need_processing) {
        // Get processing-java path
        char processing_path[MAX_PATH_LEN] = {0};

        if (first_processing_arg < argc && argv[first_processing_arg][0] != '-') {
            snprintf(processing_path, sizeof(processing_path), "%s", argv[first_processing_arg]);
        } else if (config.processing_path[0]) {
            snprintf(processing_path, sizeof(processing_path), "%s", config.processing_path);
        } else {
            fprintf(stderr, "Error: processing-java path not specified\n");
            fprintf(stderr, "Either provide it on command line or add 'processing_path' to .foldcessing config\n");
            fprintf(stderr, "Example: foldcessing.exe \"C:\\path\\to\\processing-java\" --run\n");
            if (!has_console) {
                MessageBox(NULL,
                    "processing-java path not specified.\n\n"
                    "Add 'processing_path' to your .foldcessing config file.",
                    "Foldcessing Error",
                    MB_ICONERROR | MB_OK);
            }
            return 1;
        }

        // Check if processing-java executable exists
        DWORD attribs = GetFileAttributes(processing_path);

        // If not found and path doesn't end with .exe, try adding .exe (Windows)
        if (attribs == INVALID_FILE_ATTRIBUTES && !ends_with(processing_path, ".exe")) {
            char with_exe[MAX_PATH_LEN];
            snprintf(with_exe, sizeof(with_exe), "%s.exe", processing_path);
            attribs = GetFileAttributes(with_exe);
            if (attribs != INVALID_FILE_ATTRIBUTES && !(attribs & FILE_ATTRIBUTE_DIRECTORY)) {
                strcpy(processing_path, with_exe);
            }
        }

        if (attribs == INVALID_FILE_ATTRIBUTES) {
            fprintf(stderr, "Error: processing-java not found at: %s\n", processing_path);
            fprintf(stderr, "Please check the path in your .foldcessing config or command line argument\n");
            if (!has_console) {
                char msg[MAX_PATH_LEN + 256];
                snprintf(msg, sizeof(msg),
                    "processing-java not found at:\n%s\n\n"
                    "Please check the path in your .foldcessing config.",
                    processing_path);
                MessageBox(NULL, msg, "Foldcessing Error", MB_ICONERROR | MB_OK);
            }
            return 1;
        }
        if (attribs & FILE_ATTRIBUTE_DIRECTORY) {
            fprintf(stderr, "Error: %s is a directory, not the processing-java executable\n", processing_path);
            fprintf(stderr, "The path should point to the processing-java executable file\n");
            if (!has_console) {
                char msg[MAX_PATH_LEN + 256];
                snprintf(msg, sizeof(msg),
                    "%s\n\nis a directory, not the processing-java executable.\n\n"
                    "The path should point to the processing-java executable file.",
                    processing_path);
                MessageBox(NULL, msg, "Foldcessing Error", MB_ICONERROR | MB_OK);
            }
            return 1;
        }
    }

    // Get current directory
    char current_dir[MAX_PATH_LEN];
    GetCurrentDirectory(sizeof(current_dir), current_dir);

    // Collect all .pde files
    collect_files(current_dir, "");

    // Create output directory
    char output_dir[MAX_PATH_LEN];
    snprintf(output_dir, sizeof(output_dir), "%s\\output", current_dir);
    CreateDirectory(output_dir, NULL);

    // Check if data folder exists in project root and create junction in output folder
    char data_dir[MAX_PATH_LEN];
    snprintf(data_dir, sizeof(data_dir), "%s\\data", current_dir);
    DWORD data_attribs = GetFileAttributes(data_dir);
    if (data_attribs != INVALID_FILE_ATTRIBUTES && (data_attribs & FILE_ATTRIBUTE_DIRECTORY)) {
        // Data folder exists, create junction in output folder
        char output_data_link[MAX_PATH_LEN];
        snprintf(output_data_link, sizeof(output_data_link), "%s\\data", output_dir);

        // Remove old link/folder if it exists
        RemoveDirectory(output_data_link);

        // Create junction using mklink command
        char mklink_cmd[MAX_PATH_LEN * 2];
        snprintf(mklink_cmd, sizeof(mklink_cmd), "cmd /c mklink /J \"%s\" \"%s\" >nul 2>&1",
                 output_data_link, data_dir);
        system(mklink_cmd);
    }

    // Create output file and build line mapping
    char output_file[MAX_PATH_LEN];
    snprintf(output_file, sizeof(output_file), "%s\\output.pde", output_dir);

    FILE *out = fopen(output_file, "w");
    if (!out) {
        fprintf(stderr, "Error: Cannot create output file: %s\n", output_file);
        return 1;
    }

    int current_line = 1;

    // Concatenate all files and build line mapping
    for (int i = 0; i < file_count; i++) {
        line_map[i].start_line = current_line + 1; // +1 to skip header line
        strcpy(line_map[i].relative, files[i].relative);

        // Write header comment
        fprintf(out, "//>/>/>%s\n", files[i].relative);
        current_line++;

        // Write file contents and count lines
        FILE *in = fopen(files[i].path, "r");
        if (in) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), in)) {
                fputs(line, out);
                current_line++;
            }
            fclose(in);
        }

        line_map[i].end_line = current_line - 1;

        // Blank line between files
        fprintf(out, "\n");
        current_line++;
    }

    fclose(out);

    // Store total line count for handling Java's 16-bit line number limitation
    total_lines = current_line - 1;

    printf("Foldcessing: Folded %d source files.\n\n\n", file_count);

    // Determine if we should run processing-java (already validated above)
    if (!will_need_processing) {
        return 0;
    }

    // Get processing-java path (already validated to exist)
    char processing_path[MAX_PATH_LEN] = {0};

    if (first_processing_arg < argc && argv[first_processing_arg][0] != '-') {
        snprintf(processing_path, sizeof(processing_path), "%s", argv[first_processing_arg]);
        first_processing_arg++;
    } else {
        snprintf(processing_path, sizeof(processing_path), "%s", config.processing_path);
    }

    // Try adding .exe if needed
    DWORD attribs = GetFileAttributes(processing_path);
    if (attribs == INVALID_FILE_ATTRIBUTES && !ends_with(processing_path, ".exe")) {
        char with_exe[MAX_PATH_LEN];
        snprintf(with_exe, sizeof(with_exe), "%s.exe", processing_path);
        if (GetFileAttributes(with_exe) != INVALID_FILE_ATTRIBUTES) {
            strcpy(processing_path, with_exe);
        }
    }

    char command[8192];
    int cmd_len = snprintf(command, sizeof(command), "\"%s\" --sketch=\"%s\"", processing_path, output_dir);

    // Append remaining arguments (--run, --present, etc.)
    if (argc > first_processing_arg) {
        // Use command line arguments
        for (int i = first_processing_arg; i < argc; i++) {
            cmd_len += snprintf(command + cmd_len, sizeof(command) - cmd_len, " %s", argv[i]);
        }
    } else if (config.default_action[0]) {
        // Use default action from config
        cmd_len += snprintf(command + cmd_len, sizeof(command) - cmd_len, " %s", config.default_action);
    }

    // Create pipes for stdout and stderr
    HANDLE hStdoutRead, hStdoutWrite;
    HANDLE hStderrRead, hStderrWrite;
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};

    CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0);
    CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0);
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);

    // Spawn processing-java
    STARTUPINFO si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStderrWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = {0};

    if (!CreateProcess(NULL, command, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "Failed to launch processing-java: %s\n", processing_path);
        fprintf(stderr, "The file exists but cannot be executed.\n");
        return 1;
    }

    CloseHandle(hStdoutWrite);
    CloseHandle(hStderrWrite);

    // Read and translate output in real-time (chunk-based)
    char stdout_line_buffer[MAX_LINE] = {0};
    char stderr_line_buffer[MAX_LINE] = {0};
    int stdout_pos = 0;
    int stderr_pos = 0;
    DWORD bytes_read;
    DWORD avail;
    char chunk[4096];

    while (1) {
        int activity = 0;

        // Read from stdout in chunks
        if (PeekNamedPipe(hStdoutRead, NULL, 0, NULL, &avail, NULL) && avail > 0) {
            DWORD to_read = (avail < sizeof(chunk)) ? avail : sizeof(chunk);
            if (ReadFile(hStdoutRead, chunk, to_read, &bytes_read, NULL) && bytes_read > 0) {
                activity = 1;
                for (DWORD i = 0; i < bytes_read; i++) {
                    char ch = chunk[i];
                    if (ch == '\n' || ch == '\r') {
                        if (stdout_pos > 0) {
                            stdout_line_buffer[stdout_pos] = '\0';
                            process_output_line(stdout_line_buffer);
                            printf("\n");
                            stdout_pos = 0;
                        }
                    } else if (stdout_pos < MAX_LINE - 1) {
                        stdout_line_buffer[stdout_pos++] = ch;
                    }
                }
                fflush(stdout);
            }
        }

        // Read from stderr in chunks
        if (PeekNamedPipe(hStderrRead, NULL, 0, NULL, &avail, NULL) && avail > 0) {
            DWORD to_read = (avail < sizeof(chunk)) ? avail : sizeof(chunk);
            if (ReadFile(hStderrRead, chunk, to_read, &bytes_read, NULL) && bytes_read > 0) {
                activity = 1;
                for (DWORD i = 0; i < bytes_read; i++) {
                    char ch = chunk[i];
                    if (ch == '\n' || ch == '\r') {
                        if (stderr_pos > 0) {
                            stderr_line_buffer[stderr_pos] = '\0';
                            process_output_line(stderr_line_buffer);
                            printf("\n");
                            stderr_pos = 0;
                        }
                    } else if (stderr_pos < MAX_LINE - 1) {
                        stderr_line_buffer[stderr_pos++] = ch;
                    }
                }
                fflush(stdout);
            }
        }

        DWORD exit_code;
        if (GetExitCodeProcess(pi.hProcess, &exit_code) && exit_code != STILL_ACTIVE) {
            break;
        }

        if (!activity) {
            Sleep(10);
        }
    }

    // Read any remaining output
    while (PeekNamedPipe(hStdoutRead, NULL, 0, NULL, &avail, NULL) && avail > 0) {
        DWORD to_read = (avail < sizeof(chunk)) ? avail : sizeof(chunk);
        if (ReadFile(hStdoutRead, chunk, to_read, &bytes_read, NULL) && bytes_read > 0) {
            for (DWORD i = 0; i < bytes_read; i++) {
                char ch = chunk[i];
                if (ch == '\n' || ch == '\r') {
                    if (stdout_pos > 0) {
                        stdout_line_buffer[stdout_pos] = '\0';
                        process_output_line(stdout_line_buffer);
                        printf("\n");
                        stdout_pos = 0;
                    }
                } else if (stdout_pos < MAX_LINE - 1) {
                    stdout_line_buffer[stdout_pos++] = ch;
                }
            }
        }
    }

    while (PeekNamedPipe(hStderrRead, NULL, 0, NULL, &avail, NULL) && avail > 0) {
        DWORD to_read = (avail < sizeof(chunk)) ? avail : sizeof(chunk);
        if (ReadFile(hStderrRead, chunk, to_read, &bytes_read, NULL) && bytes_read > 0) {
            for (DWORD i = 0; i < bytes_read; i++) {
                char ch = chunk[i];
                if (ch == '\n' || ch == '\r') {
                    if (stderr_pos > 0) {
                        stderr_line_buffer[stderr_pos] = '\0';
                        process_output_line(stderr_line_buffer);
                        printf("\n");
                        stderr_pos = 0;
                    }
                } else if (stderr_pos < MAX_LINE - 1) {
                    stderr_line_buffer[stderr_pos++] = ch;
                }
            }
        }
    }

    // Flush any remaining partial lines
    if (stdout_pos > 0) {
        stdout_line_buffer[stdout_pos] = '\0';
        process_output_line(stdout_line_buffer);
        printf("\n");
        fflush(stdout);
    }
    if (stderr_pos > 0) {
        stderr_line_buffer[stderr_pos] = '\0';
        process_output_line(stderr_line_buffer);
        printf("\n");
        fflush(stdout);
    }

    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(hStdoutRead);
    CloseHandle(hStderrRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Cleanup: Delete the entire output folder
    char rmdir_cmd[MAX_PATH_LEN + 50];
    snprintf(rmdir_cmd, sizeof(rmdir_cmd), "rmdir /s /q \"%s\" >nul 2>&1", output_dir);
    system(rmdir_cmd);

    return exit_code;
}
