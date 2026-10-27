#include "build_runner.h"
#include "config.h"
#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

double now_seconds()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return double(ts.tv_sec) + ts.tv_nsec * 1e-9;
}

const std::vector<std::string>& BuildRunner::steps()
{
	static const std::vector<std::string> s = {"deps", "verify", "convert", "mods", "tables", "lift", "compile", "link", "text", "check"};
	return s;
}

bool BuildRunner::start(const std::string& repo_dir, const std::vector<std::string>& args)
{
	if (running())
		return false;
	repo = repo_dir;
	current.clear();
	failed.clear();
	done.clear();
	took.clear();
	lines.clear();
	partial.clear();
	compiled = to_compile = 0;
	exit_code = 0;
	int p[2];
	if (pipe(p))
		return false;
	pid = fork();
	if (pid == 0)
	{
		setpgid(0, 0);                       // its own group: stop() ends the build and the compilers it started
		dup2(p[1], 1);
		dup2(p[1], 2);
		close(p[0]);
		close(p[1]);
		if (chdir(repo.c_str()))
			_exit(127);
		setenv("PYTHONUNBUFFERED", "1", 1);
		std::vector<std::string> argv = build_command(repo, "setup");
		argv.insert(argv.end(), args.begin(), args.end());
		std::vector<char*> a;
		for (std::string& s : argv)
			a.push_back(s.data());
		a.push_back(nullptr);
		execvp(a[0], a.data());
		std::printf("%s cannot be started: %s\n", a[0], std::strerror(errno));
		_exit(127);
	}
	close(p[1]);
	if (pid < 0)
	{
		close(p[0]);
		return false;
	}
	fd = p[0];
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
	started_at = now_seconds();
	return true;
}

void BuildRunner::stop()
{
	if (pid > 0)
		kill(-pid, SIGTERM);
}

void BuildRunner::line(const std::string& l)
{
	// "[i/10] step ...", "[i/10] step: ok  12.3s", "[i/10] step: FAILED"
	if (l.size() > 3 && l[0] == '[' && l.find('/') < l.find(']'))
	{
		size_t close = l.find(']');
		std::string rest = l.substr(close + 2);
		std::string name = rest.substr(0, rest.find_first_of(" :"));
		if (size_t ok = rest.find(": ok"); ok != std::string::npos)
		{
			done.push_back(name);
			std::string t = rest.substr(ok + 4);
			took[name] = t.substr(std::min(t.size(), t.find_first_not_of(' ')));
			current.clear();
		}
		else if (rest.find(": FAILED") != std::string::npos)
		{
			failed = name;
			current.clear();
		}
		else
		{
			current = name;
			if (name == "compile")                   // only what this run writes counts (the log is appended to)
			{
				std::ifstream f(repo + "/build/logs/compile.log", std::ios::binary | std::ios::ate);
				compile_log_start = f ? std::streamoff(f.tellg()) : 0;
			}
		}
		return;
	}
	lines.push_back(l);
	if (lines.size() > 200)
		lines.erase(lines.begin(), lines.begin() + 100);
}

void BuildRunner::read_compile_log()
{
	std::ifstream f(repo + "/build/logs/compile.log", std::ios::binary | std::ios::ate);
	if (!f)
		return;
	std::streamoff size = f.tellg();
	f.seekg(std::max<long long>(compile_log_start, size > 4096 ? size - 4096 : 0));
	std::string tail((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	// the last "[done/total] 12s" line compile_main.py printed
	for (size_t at = tail.find("\n["); at != std::string::npos; at = tail.find("\n[", at + 1))
	{
		int n = 0, total = 0;
		if (std::sscanf(tail.c_str() + at + 1, "[%d/%d]", &n, &total) == 2 && total > 0)
		{
			compiled = n;
			to_compile = total;
		}
	}
}

bool BuildRunner::poll()
{
	if (pid <= 0)
		return false;
	bool changed = false;
	char buf[4096];
	for (ssize_t n; (n = read(fd, buf, sizeof buf)) > 0;)
	{
		partial.append(buf, size_t(n));
		changed = true;
	}
	for (size_t nl; (nl = partial.find('\n')) != std::string::npos;)
	{
		std::string l = partial.substr(0, nl);
		if (!l.empty() && l.back() == '\r')
			l.pop_back();
		partial.erase(0, nl + 1);
		line(l);
	}
	if (current == "compile" && now_seconds() - last_log_check > .5)
	{
		last_log_check = now_seconds();
		int before = compiled;
		read_compile_log();
		changed |= compiled != before;
	}
	int status = 0;
	if (waitpid(pid, &status, WNOHANG) == pid)
	{
		for (ssize_t n; (n = read(fd, buf, sizeof buf)) > 0;)
			partial.append(buf, size_t(n));
		if (!partial.empty())
			line(partial);
		partial.clear();
		close(fd);
		fd = -1;
		pid = -1;
		exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
		current.clear();
		changed = true;
	}
	return changed;
}
