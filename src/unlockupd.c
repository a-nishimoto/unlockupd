/*
	unlockupd 1.0.2
	Copyright (c) 2004-2009 John Bafford
	http://bafford.com/software/unlockupd/
	
	unlockupd is a simple threads-based daemon that checks to see if
	lookupd has died, and if so, kills it so it can respawn.

    This file is part of Unlockupd.

    Unlockupd is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Unlockupd is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Unlockupd.  If not, see <http://www.gnu.org/licenses/>.
*/

#define UNLOCKUPD_VERSION "1.0.2"
#define UNLOCKUPD_COPYRIGHT "2004-2009 John Bafford <http://bafford.com/>"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include <netdb.h>
#include <pwd.h>

#include "getps.h"

/*
	We check lookupd every CHECK_DELAY seconds.
	It has DEATH_DELAY seconds to respond, or we kill it.
*/

enum {
	CHECK_DELAY = 15,
	DEATH_DELAY = 15,
	MAX_LOOKUPD_KILLS = 3,
};

/*
	GLOBAL VARIABLES
*/

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int doQuit = 0;

volatile int lookupdOK = 1;

/*
	CODE
*/

void LogKillMsg(const char* name, pid_t pid, int tryNum)
{
	syslog(LOG_CRIT, "killing %s[%d] (try #%d)", name, pid, tryNum);
}

/*
	Read the pid out of the lookupd.pid file. Is there a safer way to do this?
*/
pid_t GetLookupdPID()
{
	FILE *fd = fopen("/var/run/lookupd.pid", "r");
	int pidX;
	pid_t pid;
	
	fscanf(fd, "%d", &pidX);
	fclose(fd);
	
	pid = pidX;
	
	if(pid == pidX)
		return pid;
	else
		return 0;
}

/*
	Kill a process. Return 0 on success, errno on failure.
*/
int DoKill(const pid_t pid, const char* name)
{
	int error = 0;
	int killErr = kill(pid, SIGKILL);
	
	if(killErr)
	{
		error = errno;
		
		//The error codes man 2 kill says kill() returns
		switch(error)
		{
			//Reasonable to get this error
			case ESRCH:
				syslog(LOG_CRIT, "killing %s[%d] failed: no such process", name, pid);
				break;
			
			//Absolutely completely totally unreasonable to get these errors
			case EPERM:
				syslog(LOG_CRIT, "killing %s[%d] failed: don't have the permissions to kill this process!", name, pid);
				break;
			
			case EINVAL:
				syslog(LOG_CRIT, "an unexpected error occured: %d is not a valid signal number. If you see this, something is very wrong...", SIGKILL);
				break;
			
			default:
				syslog(LOG_CRIT, "an unexpected error occured: %d", error);
				break;
		}
	}
	
	return error;
}

/*
	Find the pid for crashreporterd and kill it.
*/
void KillCrashreporterd()
{
	static pid_t lastPID = 0;
	static unsigned int tryNum = 0;
	pid_t pid = FindProcess("crashreporterd");
	
	if(pid)
	{
		int killErr;
		
		if(pid == lastPID)
			tryNum++;
		else
		{
			lastPID = pid;
			tryNum = 1;
		}
		
		LogKillMsg("crashreporterd", pid, tryNum);
		
		killErr = DoKill(pid, "crashreporterd");
	}
	else
		syslog(LOG_CRIT, "Unable to get lookupd's pid. Sorry, can't help you now...");
}

/*
	Attempt to kill lookupd. Return true if we think we were successful, or false if we don't think we were.
	
	If this fails, it has been suggested to
	
	* kill crashreporterd
	* kill DirectoryService
	
*/
char KillLookupd()
{
	static pid_t lastPID = 0;
	static unsigned int tryNum = 0;
	pid_t pid = GetLookupdPID();
	
	syslog(LOG_CRIT, "pid %d", pid);
	
	if(pid)
	{
		int killErr;
		
		if(pid == lastPID)
			tryNum++;
		else
		{
			lastPID = pid;
			tryNum = 1;
		}
		
		if(tryNum < MAX_LOOKUPD_KILLS)
		{
			LogKillMsg("lookupd", pid, tryNum);
			
			killErr = DoKill(pid, "lookupd");
			
			return !!killErr;
		}
		else
		{
			//Try more drastic measures
			KillCrashreporterd();
		}
	}
	else
		syslog(LOG_CRIT, "Unable to get lookupd's pid. Sorry, can't help you now...");
	
	return 0;
}

/*
	We could call getservbyname() with invalid parameters, which bypasses the
	libc cache and forces lookupd to respond. Unfortunately, that operation is
	unreasonably expensive, so we can't use that.
	--> getservbyname("unlockupd", NULL);
	
	Alternatively, we can use getservbyname("rtmp", NULL), which retrieves the
	first item in the /etc/services file and uses negligible CPU time, as long as we
	don't do it any faster than the service cache TTL, which is 10 seconds.
	
	Alternatively, we can also use getpwname(""), however, this requires a search
	of the entire users database, which is faster than getservbyname("invalid")
	on a normal system, but would heavily suck on LDAP or other network-enabled
	NetInfo setups.
*/

void* cattleprod(void* cowpoke)
{
	struct timespec ts = {CHECK_DELAY, 0};
	
	for(;;)
	{
		pthread_mutex_lock(&mutex);
		lookupdOK = 0;
		pthread_mutex_unlock(&mutex);
		
		getservbyname("rtmp", NULL);
		
		pthread_mutex_lock(&mutex);
		lookupdOK = 1;
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mutex);
		
		if(doQuit)
			break;
		
		//Sleep until it's time to check again
		nanosleep(&ts, NULL);
	}
	
	return 0;
}

int main(int argn, char* argv[])
{
	pthread_t cattleProd;
	int err;
	struct timespec ts = {0, 0};
	uid_t curUser;
	
	//Print out version information
	if(argn > 1)
	{
		printf("unlockupd " UNLOCKUPD_VERSION " (c) " UNLOCKUPD_COPYRIGHT "\n");
		exit(0);
	}
	
	//First, let's make sure we are running as root
	curUser = geteuid();
	if(curUser != 0)
	{
		fprintf(stderr, "unlockupd requires root permissions to run.\n");
		exit(1);
	}
	
	openlog("unlockupd", LOG_PERROR | LOG_PID, LOG_DAEMON);
	syslog(LOG_INFO, "unlockupd starting up");
	
	err = pthread_create(&cattleProd, NULL, cattleprod, NULL);
	
	if(err)
	{
		syslog(LOG_CRIT, "Could not create cattleprod. You should never see this error message. Exiting.");
		return 1;
	}
	
	pthread_mutex_lock(&mutex);
	for(;;)
	{
		ts.tv_sec = time(NULL) + DEATH_DELAY + CHECK_DELAY;
		pthread_cond_timedwait(&cond, &mutex, &ts);
		
		if(!lookupdOK)
			KillLookupd();
		
		if(doQuit)
			break;
	}
	pthread_mutex_unlock(&mutex);
	
	pthread_join(cattleProd, NULL);
	
	syslog(LOG_INFO, "unlockupd exiting");
	
	return 0;
}
