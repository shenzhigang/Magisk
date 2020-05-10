#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>

#include <magisk.hpp>
#include <utils.hpp>
#include <selinux.hpp>
#include <db.hpp>

#include "magiskhide.hpp"

using namespace std;

static pthread_t proc_monitor_thread;
static bool hide_state = false;
string system_mnt_type;
string system_root_mnt_type;

// This locks the 2 variables above
static pthread_mutex_t hide_state_lock = PTHREAD_MUTEX_INITIALIZER;

// Leave /proc fd opened as we're going to read from it repeatedly
static DIR *procfp;
void crawl_procfs(const function<bool(int)> &fn) {
	rewinddir(procfp);
	crawl_procfs(procfp, fn);
}

void crawl_procfs(DIR *dir, const function<bool(int)> &fn) {
	struct dirent *dp;
	int pid;
	while ((dp = readdir(dir))) {
		pid = parse_int(dp->d_name);
		if (pid > 0 && !fn(pid))
			break;
	}
}

bool hide_enabled() {
	mutex_guard g(hide_state_lock);
	return hide_state;
}

void set_hide_state(bool state) {
	mutex_guard g(hide_state_lock);
	hide_state = state;
}

static bool proc_name_match(int pid, const char *name) {
	char buf[4019];
	sprintf(buf, "/proc/%d/cmdline", pid);
	if (FILE *f = fopen(buf, "re")) {
		fgets(buf, sizeof(buf), f);
		fclose(f);
		if (strcmp(buf, name) == 0)
			return true;
	}
	return false;
}

static void kill_process(const char *name, bool multi = false) {
	crawl_procfs([=](int pid) -> bool {
		if (proc_name_match(pid, name)) {
			if (kill(pid, SIGTERM) == 0)
				LOGD("hide_utils: killed PID=[%d] (%s)\n", pid, name);
			return multi;
		}
		return true;
	});
}

static bool validate(const char *s) {
	bool dot = false;
	for (char c; (c = *s); ++s) {
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') || c == '_' || c == ':') {
			continue;
		}
		if (c == '.') {
			dot = true;
			continue;
		}
		return false;
	}
	return dot;
}

static void init_list(int uid, const char *pkg, const char *proc);

static int add_list(int uid, const char *pkg, const char *proc = "") {
	if (proc[0] == '\0')
		proc = pkg;

	if (uid % 100000 < 10000 || uid % 100000 > 19999 || !validate(pkg) || !validate(proc))
		return HIDE_INVALID_PKG;

	for (auto &hide : hide_set)
		if (hide.first == uid && hide.second.first == pkg && hide.second.second == proc)
			return HIDE_ITEM_EXIST;

	// Add to database
	char sql[4096];
	snprintf(sql, sizeof(sql),
			 "INSERT INTO sulist (uid, package_name, process, logging, notification) "
			 "VALUES('%d', '%s', '%s', 1, 1)", uid, pkg, proc);
	char *err = db_exec(sql);
	db_err_cmd(err, return DAEMON_ERROR);

	LOGI("su_list add: UID=%d [%s/%s]\n", uid, pkg, proc);

	// Critical region
	{
		mutex_guard lock(monitor_lock);
		init_list(uid, pkg, proc);
	}

	return DAEMON_SUCCESS;
}

int add_list(int client) {
	int uid = read_int(client);
	char *pkg = read_string(client);
	char *proc = read_string(client);
	int ret = add_list(uid, pkg, proc);
	free(pkg);
	free(proc);
	update_uid_map();
	return ret;
}

static int rm_list(int uid, const char *pkg, const char *proc = "") {
	{
		// Critical region
		mutex_guard lock(monitor_lock);
		bool remove = false;
		for (auto it = hide_set.begin(); it != hide_set.end();) {
			if (it->first == uid && it->second.first == pkg &&
				(proc[0] == '\0' || it->second.second == proc)) {
				remove = true;
				LOGI("su_list rm: UID=%d [%s]\n", it->first, it->second.second.data());
				it = hide_set.erase(it);
			} else {
				++it;
			}
		}
		if (!remove)
			return HIDE_ITEM_NOT_EXIST;
	}

	char sql[4096];
	if (proc[0] == '\0')
		snprintf(sql, sizeof(sql), "DELETE FROM sulist WHERE uid='%d' AND package_name='%s'",
				uid, pkg);
	else snprintf(sql, sizeof(sql),
		         "DELETE FROM sulist WHERE uid='%d' AND package_name='%s' AND process='%s'",
		         uid, pkg, proc);
	char *err = db_exec(sql);
	db_err(err);
	return DAEMON_SUCCESS;
}

int rm_list(int client) {
	int uid = read_int(client);
	char *pkg = read_string(client);
	char *proc = read_string(client);
	int ret = rm_list(uid, pkg, proc);
	free(pkg);
	free(proc);
	if (ret == DAEMON_SUCCESS)
		update_uid_map();
	return ret;
}

static void init_list(int uid, const char *pkg, const char *proc) {
	if (multiuser_mode == -1) {
		db_settings dbs;
		get_db_settings(dbs, SU_MULTIUSER_MODE);
		multiuser_mode = dbs[SU_MULTIUSER_MODE];
	}

	if (uid == 2000 || (multiuser_mode != MULTIUSER_MODE_USER && uid >= 100000)) {
		LOGW("su_list init: ignore init UID=%d %s", uid, pkg);
		return;
	}

	LOGI("su_list init: UID=%d [%s/%s]\n", uid, pkg, proc);
	hide_set.emplace(uid, make_pair(pkg, proc));
	kill_process(proc);
}

#define SNET_PROC    "com.google.android.gms.unstable"
#define GMS_PKG      "com.google.android.gms"
#define MICROG_PKG   "org.microg.gms.droidguard"

static bool init_list() {
	LOGD("su_list: initialize\n");

	char *err = db_exec("SELECT * FROM sulist", [](db_row &row) -> bool {
		init_list(parse_int(row["uid"]), row["package_name"].data(), row["process"].data());
		return true;
	});
	db_err_cmd(err, return false);

	// If Android Q+, also kill blastula pool
	if (SDK_INT >= 29) {
		kill_process("usap32", true);
		kill_process("usap64", true);
	}

	kill_process(SNET_PROC);
	kill_process(GMS_PKG);
	kill_process(MICROG_PKG);

	db_strings str;
	struct stat st{};
	get_db_strings(str, SU_MANAGER);
	if (validate_manager(str[SU_MANAGER], 0, &st)) {
		init_list(st.st_uid, str[SU_MANAGER].data(), str[SU_MANAGER].data());
	}

	update_uid_map();
	return true;
}

void ls_list(int client) {
	FILE *out = fdopen(recv_fd(client), "a");
	for (auto &hide : hide_set)
		fprintf(out, "%d|%s|%s\n", hide.first, hide.second.first.data(), hide.second.second.data());
	fclose(out);
	write_int(client, DAEMON_SUCCESS);
	close(client);
}

static void update_hide_config() {
	char sql[64];
	sprintf(sql, "REPLACE INTO settings (key,value) VALUES('%s',%d)",
			DB_SETTING_KEYS[HIDE_CONFIG], hide_state);
	char *err = db_exec(sql);
	db_err(err);
}

static void copy_magisk_tmp() {
	string tmp_dir;
	char buf[8];
	gen_rand_str(buf, sizeof(buf));
	tmp_dir = "/dev/"s + buf;
	xmkdir(tmp_dir.data(), 0);
	setfilecon(tmp_dir.data(), "u:object_r:tmpfs:s0");

	SUMODULE = tmp_dir;
	chdir(tmp_dir.data());

	xmkdir(INTLROOT, 0755);
	xmkdir(MIRRDIR, 0);
	xmkdir(BLOCKDIR, 0);

	for (auto file : {"magisk", "magiskinit"}) {
		auto src = MAGISKTMP + "/"s + file;
		auto dest = tmp_dir + "/"s + file;
		cp_afc(src.data(), dest.data());
		if (file == "magisk"sv) setfilecon(dest.data(), "u:object_r:" SEPOL_EXEC_TYPE ":s0");
	}

	parse_mnt("/proc/mounts", [&](mntent *me) {
		struct stat st{};
		if ((me->mnt_dir == string_view("/system")) && me->mnt_type != "tmpfs"sv &&
			lstat(me->mnt_dir, &st) == 0) {
			mknod(BLOCKDIR "/system", S_IFBLK | 0600, st.st_dev);
			xmkdir(MIRRDIR "/system", 0755);
			system_mnt_type = me->mnt_type;
			return false;
		}
		return true;
	});
	if (access(MIRRDIR "/system", F_OK) != 0) {
		xsymlink("./system_root/system", MIRRDIR "/system");
		parse_mnt("/proc/mounts", [&](mntent *me) {
			struct stat st{};
			if ((me->mnt_dir == string_view("/")) && me->mnt_type != "rootfs"sv &&
				stat("/", &st) == 0) {
				mknod(BLOCKDIR "/system_root", S_IFBLK | 0600, st.st_dev);
				xmkdir(MIRRDIR "/system_root", 0755);
				system_root_mnt_type = me->mnt_type;
				return false;
			}
			return true;
		});
	}

	chdir("/");
}

int launch_magiskhide() {
	mutex_guard g(hide_state_lock);

	if (SDK_INT < 19)
		return DAEMON_ERROR;

	if (hide_state)
		return HIDE_IS_ENABLED;

	if (access("/proc/1/ns/mnt", F_OK) != 0)
		return HIDE_NO_NS;

	if (procfp == nullptr && (procfp = opendir("/proc")) == nullptr)
		return DAEMON_ERROR;

	LOGI("* Starting MagiskHide\n");

	// Initialize the mutex lock
	pthread_mutex_init(&monitor_lock, nullptr);

	// Initialize the hide list
	if (!init_list())
		return DAEMON_ERROR;

	copy_magisk_tmp();

	hide_sensitive_props();
	if (DAEMON_STATE >= STATE_BOOT_COMPLETE || DAEMON_STATE == STATE_NONE)
		hide_late_sensitive_props();

	// Start monitoring
	void *(*start)(void*) = [](void*) -> void* { proc_monitor(); return nullptr; };
	if (xpthread_create(&proc_monitor_thread, nullptr, start, nullptr))
		return DAEMON_ERROR;

	hide_state = true;
	update_hide_config();
	return DAEMON_SUCCESS;
}

int stop_magiskhide() {
	mutex_guard g(hide_state_lock);

	if (hide_state) {
		LOGI("* Stopping MagiskHide\n");
		pthread_kill(proc_monitor_thread, SIGTERMTHRD);
	}

	hide_state = false;
	update_hide_config();
	rm_rf(SUMODULE.data());
	return DAEMON_SUCCESS;
}

void auto_start_magiskhide() {
	if (hide_enabled()) {
		return;
	} else if (SDK_INT >= 19) {
		db_settings dbs;
		get_db_settings(dbs, HIDE_CONFIG);
		if (dbs[HIDE_CONFIG])
			launch_magiskhide();
	}
}

void test_proc_monitor() {
	if (procfp == nullptr && (procfp = opendir("/proc")) == nullptr)
		exit(1);
	proc_monitor();
}
