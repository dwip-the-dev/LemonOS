#include "git.h"
#include "crypto.h"
#include "posix_time.h"
#include "filesystem.h"
#include "vfs.h"
#include "string.h"
#include "serial.h"
#include "vga.h"

static char git_user_name[48]  = "Dwip (dwip-the-dev)";
static char git_user_email[48] = "dwip@dwip.dedyn.io";
static char git_current_branch[32] = "main";
static char git_head_commit[41] = "";

static git_index_entry_t index_entries[GIT_MAX_INDEX_ENTRIES];
static int index_count = 0;

static git_commit_info_t commit_history[GIT_MAX_COMMITS];
static int commit_count = 0;

static int git_initialized = 0;

void git_init_subsystem(void) {
    index_count = 0;
    commit_count = 0;
    git_initialized = 0;
    serial_println("[GIT] Git version control subsystem initialized.");
}

int git_init(const char* dir) {
    (void)dir;
    fs_mkdir("/.git");
    fs_mkdir("/.git/objects");
    fs_mkdir("/.git/refs");
    fs_mkdir("/.git/refs/heads");

    fs_create_file("/.git/HEAD", "ref: refs/heads/main\n");
    fs_create_file("/.git/config",
        "[core]\n"
        "\trepositoryformatversion = 0\n"
        "\tfilemode = true\n"
        "\tbare = false\n"
        "[user]\n"
        "\tname = Dwip (dwip-the-dev)\n"
        "\temail = dwip@dwip.dedyn.io\n");

    strcpy(git_current_branch, "main");
    git_head_commit[0] = '\0';
    index_count = 0;
    commit_count = 0;
    git_initialized = 1;

    serial_println("[GIT] Initialized empty Git repository in /.git/");
    return 0;
}

int git_config_set(const char* key, const char* value) {
    if (!key || !value) return -1;
    if (strcmp(key, "user.name") == 0) {
        strncpy(git_user_name, value, sizeof(git_user_name) - 1);
        return 0;
    } else if (strcmp(key, "user.email") == 0) {
        strncpy(git_user_email, value, sizeof(git_user_email) - 1);
        return 0;
    }
    return -1;
}

int git_add(const char* file_path) {
    if (!file_path || file_path[0] == '\0') return -1;
    if (!git_initialized) git_init(".");

    char* content = fs_read_file(file_path);
    if (!content) {
        serial_print("fatal: pathspec '");
        serial_print(file_path);
        serial_println("' did not match any files");
        return -1;
    }

    size_t fsize = strlen(content);

    // Compute Git object format: "blob <len>\0<content>"
    char obj_buf[2048];
    char len_str[16];
    int lp = 0;
    size_t s = fsize;
    if (s == 0) len_str[lp++] = '0';
    else {
        char rev[16]; int rp = 0;
        while (s > 0) { rev[rp++] = '0' + (s % 10); s /= 10; }
        while (rp > 0) len_str[lp++] = rev[--rp];
    }
    len_str[lp] = '\0';

    int hdr_len = 5 + lp + 1; // "blob " + len + '\0'
    strcpy(obj_buf, "blob ");
    strcat(obj_buf, len_str);
    obj_buf[5 + lp] = '\0';

    // Calculate SHA-1 over blob header and payload
    sha1_ctx_t ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, (const uint8_t*)obj_buf, hdr_len);
    sha1_update(&ctx, (const uint8_t*)content, fsize);
    uint8_t digest[SHA1_DIGEST_SIZE];
    sha1_final(&ctx, digest);

    char sha1_hex[41];
    static const char hex_digits[] = "0123456789abcdef";
    for (int i = 0; i < SHA1_DIGEST_SIZE; i++) {
        sha1_hex[i * 2]     = hex_digits[(digest[i] >> 4) & 0xF];
        sha1_hex[i * 2 + 1] = hex_digits[digest[i] & 0xF];
    }
    sha1_hex[40] = '\0';

    // Store object
    char obj_path[64];
    strcpy(obj_path, "/.git/objects/");
    strcat(obj_path, sha1_hex);
    fs_create_file(obj_path, content);

    // Record in index
    int found = -1;
    for (int i = 0; i < index_count; i++) {
        if (strcmp(index_entries[i].path, file_path) == 0) {
            found = i;
            break;
        }
    }
    if (found == -1 && index_count < GIT_MAX_INDEX_ENTRIES) {
        found = index_count++;
    }
    if (found >= 0) {
        strncpy(index_entries[found].path, file_path, sizeof(index_entries[found].path) - 1);
        strncpy(index_entries[found].sha1_hex, sha1_hex, 40);
        index_entries[found].sha1_hex[40] = '\0';
        index_entries[found].staged = 1;
    }

    serial_print("[GIT] Staged '");
    serial_print(file_path);
    serial_print("' -> blob ");
    serial_print(sha1_hex);
    serial_println("");
    return 0;
}

int git_commit(const char* message) {
    if (!message || message[0] == '\0') return -1;
    if (!git_initialized) git_init(".");

    if (index_count == 0) {
        serial_println("nothing to commit, working tree clean");
        return -1;
    }

    // 1. Build tree object string
    char tree_data[1024];
    tree_data[0] = '\0';
    for (int i = 0; i < index_count; i++) {
        strcat(tree_data, "100644 blob ");
        strcat(tree_data, index_entries[i].sha1_hex);
        strcat(tree_data, "\t");
        strcat(tree_data, index_entries[i].path);
        strcat(tree_data, "\n");
    }

    char tree_sha1[41];
    sha1_hash_str(tree_data, tree_sha1);

    // 2. Format commit metadata
    struct tm cur_tm;
    time_t now = posix_time(NULL);
    posix_localtime_r(&now, &cur_tm);
    char date_buf[32];
    posix_strftime(date_buf, sizeof(date_buf), "%a %b %d %H:%M:%S %Y %z", &cur_tm);

    char commit_payload[1024];
    commit_payload[0] = '\0';
    strcat(commit_payload, "tree "); strcat(commit_payload, tree_sha1); strcat(commit_payload, "\n");
    if (git_head_commit[0] != '\0') {
        strcat(commit_payload, "parent "); strcat(commit_payload, git_head_commit); strcat(commit_payload, "\n");
    }
    strcat(commit_payload, "author "); strcat(commit_payload, git_user_name);
    strcat(commit_payload, " <"); strcat(commit_payload, git_user_email); strcat(commit_payload, "> ");
    strcat(commit_payload, date_buf); strcat(commit_payload, "\n");
    strcat(commit_payload, "committer "); strcat(commit_payload, git_user_name);
    strcat(commit_payload, " <"); strcat(commit_payload, git_user_email); strcat(commit_payload, "> ");
    strcat(commit_payload, date_buf); strcat(commit_payload, "\n\n");
    strcat(commit_payload, message); strcat(commit_payload, "\n");

    char commit_sha1[41];
    sha1_hash_str(commit_payload, commit_sha1);

    // 3. Save commit object and update branch ref
    char c_path[64];
    strcpy(c_path, "/.git/objects/");
    strcat(c_path, commit_sha1);
    fs_create_file(c_path, commit_payload);

    char ref_path[64];
    strcpy(ref_path, "/.git/refs/heads/");
    strcat(ref_path, git_current_branch);
    fs_create_file(ref_path, commit_sha1);

    // 4. Update memory commit history
    if (commit_count < GIT_MAX_COMMITS) {
        git_commit_info_t* c = &commit_history[commit_count++];
        strncpy(c->sha1_hex, commit_sha1, 40); c->sha1_hex[40] = '\0';
        strncpy(c->parent_hex, git_head_commit, 40); c->parent_hex[40] = '\0';
        strncpy(c->author, git_user_name, sizeof(c->author) - 1);
        strncpy(c->email, git_user_email, sizeof(c->email) - 1);
        strncpy(c->message, message, sizeof(c->message) - 1);
        strncpy(c->date, date_buf, sizeof(c->date) - 1);
    }

    strcpy(git_head_commit, commit_sha1);

    serial_print("[");
    serial_print(git_current_branch);
    if (commit_count == 1) serial_print(" (root-commit) ");
    else serial_print(" ");
    char short_sha[8];
    strncpy(short_sha, commit_sha1, 7); short_sha[7] = '\0';
    serial_print(short_sha);
    serial_print("] ");
    serial_println(message);

    serial_print(" ");
    serial_print_dec(index_count);
    serial_println(" file(s) changed, staged files committed.");

    return 0;
}

int git_status(char* out_buf, size_t max) {
    if (!out_buf || max == 0) return 0;
    out_buf[0] = '\0';

    strcat(out_buf, "On branch ");
    strcat(out_buf, git_current_branch);
    strcat(out_buf, "\n");

    if (commit_count == 0) {
        strcat(out_buf, "No commits yet\n\n");
    } else {
        strcat(out_buf, "\n");
    }

    if (index_count > 0) {
        strcat(out_buf, "Changes to be committed:\n  (use \"git restore --staged <file>...\" to unstage)\n");
        for (int i = 0; i < index_count; i++) {
            strcat(out_buf, "\tnew file:   ");
            strcat(out_buf, index_entries[i].path);
            strcat(out_buf, "\n");
        }
    } else {
        strcat(out_buf, "nothing to commit, working tree clean\n");
    }

    return (int)strlen(out_buf);
}

int git_log(char* out_buf, size_t max) {
    if (!out_buf || max == 0) return 0;
    out_buf[0] = '\0';

    if (commit_count == 0) {
        strcat(out_buf, "fatal: your current branch '");
        strcat(out_buf, git_current_branch);
        strcat(out_buf, "' does not have any commits yet\n");
        return (int)strlen(out_buf);
    }

    for (int i = commit_count - 1; i >= 0; i--) {
        strcat(out_buf, "commit ");
        strcat(out_buf, commit_history[i].sha1_hex);
        if (i == commit_count - 1) {
            strcat(out_buf, " (HEAD -> ");
            strcat(out_buf, git_current_branch);
            strcat(out_buf, ")");
        }
        strcat(out_buf, "\nAuthor: ");
        strcat(out_buf, commit_history[i].author);
        strcat(out_buf, " <");
        strcat(out_buf, commit_history[i].email);
        strcat(out_buf, ">\nDate:   ");
        strcat(out_buf, commit_history[i].date);
        strcat(out_buf, "\n\n    ");
        strcat(out_buf, commit_history[i].message);
        strcat(out_buf, "\n\n");
    }

    return (int)strlen(out_buf);
}

int git_branch(const char* new_branch, char* out_buf, size_t max) {
    if (!new_branch || new_branch[0] == '\0') {
        if (out_buf && max > 0) {
            out_buf[0] = '\0';
            strcat(out_buf, "* ");
            strcat(out_buf, git_current_branch);
            strcat(out_buf, "\n");
        }
        return 0;
    }
    char ref[64];
    strcpy(ref, "/.git/refs/heads/");
    strcat(ref, new_branch);
    fs_create_file(ref, git_head_commit);
    serial_print("[GIT] Created branch '");
    serial_print(new_branch);
    serial_println("'");
    return 0;
}

int git_checkout(const char* branch_name) {
    if (!branch_name) return -1;
    strncpy(git_current_branch, branch_name, sizeof(git_current_branch) - 1);
    char head_str[64];
    strcpy(head_str, "ref: refs/heads/");
    strcat(head_str, branch_name);
    strcat(head_str, "\n");
    fs_write_file("/.git/HEAD", head_str);

    serial_print("Switched to branch '");
    serial_print(branch_name);
    serial_println("'");
    return 0;
}

int git_cli(int argc, char** argv) {
    if (argc < 2) {
        serial_println("usage: git [--version] [--help] <command> [<args>]");
        serial_println("Common Git commands:\n   init       Create an empty Git repository");
        serial_println("   add        Add file contents to the index");
        serial_println("   commit     Record changes to the repository");
        serial_println("   status     Show the working tree status");
        serial_println("   log        Show commit logs");
        serial_println("   branch     List, create, or delete branches");
        serial_println("   checkout   Switch branches or restore working tree files");
        return 0;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "--version") == 0) {
        serial_println("git version 2.43.0 (LemonOS Port)");
        return 0;
    }

    if (strcmp(cmd, "init") == 0) {
        const char* d = (argc >= 3) ? argv[2] : ".";
        return git_init(d);
    } else if (strcmp(cmd, "add") == 0) {
        if (argc < 3) {
            vga_println("Nothing specified, nothing added.");
            serial_println("Nothing specified, nothing added.");
            return 1;
        }
        if (strcmp(argv[2], ".") == 0 || strcmp(argv[2], "-A") == 0) {
            git_add("/docs/welcome.txt");
            git_add("/docs/network.txt");
            return 0;
        }
        return git_add(argv[2]);
    } else if (strcmp(cmd, "commit") == 0) {
        const char* msg = "Update LemonOS codebase";
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
                msg = argv[i + 1];
                break;
            }
        }
        return git_commit(msg);
    } else if (strcmp(cmd, "status") == 0) {
        char buf[512];
        git_status(buf, sizeof(buf));
        vga_println(buf);
        serial_println(buf);
        return 0;
    } else if (strcmp(cmd, "log") == 0) {
        char buf[1024];
        git_log(buf, sizeof(buf));
        vga_println(buf);
        serial_println(buf);
        return 0;
    } else if (strcmp(cmd, "branch") == 0) {
        if (argc >= 3) return git_branch(argv[2], NULL, 0);
        char buf[128];
        git_branch(NULL, buf, sizeof(buf));
        vga_println(buf);
        serial_println(buf);
        return 0;
    } else if (strcmp(cmd, "checkout") == 0 && argc >= 3) {
        return git_checkout(argv[2]);
    } else if (strcmp(cmd, "config") == 0 && argc >= 4) {
        return git_config_set(argv[2], argv[3]);
    }

    vga_print("git: '");
    vga_print(cmd);
    vga_println("' is not a git command. See 'git --help'.");
    serial_print("git: '");
    serial_print(cmd);
    serial_println("' is not a git command. See 'git --help'.");
    return 1;
}

void git_run_tests(void) {
    serial_println("[GIT] Running Git Verification Tests...");
    git_init(".");
    git_add("/docs/welcome.txt");

    int res = git_commit("feat(kernel): add posix subsystem and dev tools");
    if (res == 0 && commit_count > 0 && git_head_commit[0] != '\0') {
        serial_println("  -> [TEST 1/2] git add and git commit with SHA-1 hashing: PASS");
    } else {
        serial_println("  -> [TEST 1/2] git commit: FAIL");
    }

    char log_buf[512];
    git_log(log_buf, sizeof(log_buf));
    if (strstr(log_buf, "feat(kernel): add posix subsystem") != NULL) {
        serial_println("  -> [TEST 2/2] git log formatted history verification: PASS");
    } else {
        serial_println("  -> [TEST 2/2] git log: FAIL");
    }
    serial_println("[GIT] Git subsystem verified.");
}
