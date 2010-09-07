/*
 * Copyright (C) 2010 Bluecherry, LLC
 *
 * Confidential, all rights reserved. No distribution is permitted.
 */

#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <libbluecherry.h>

/* These must match the defs in the SQL db */
static const char *level_to_str[] = {
	"info", "warn", "alrm", "crit"
};

static const char *cam_type_to_str[] = {
	"motion", "not found", "video signal loss", "audio signal loss"
};

static const char *sys_type_to_str[] = {
	"disk-space", "crash", "boot", "shutdown", "reboot", "power-outage"
};

/* Linked list if events that failed to write */
static BC_DECLARE_LIST(cam_event_queue);
static BC_DECLARE_LIST(sys_event_queue);
static struct bc_db_handle *bcdb;

struct bc_event_cam {
	int id;
	const char *level;
	const char *type;
	time_t start_time;
	time_t end_time;
	int inserted;
	struct bc_list_struct list;
};

struct bc_event_sys {
	const char *level;
	const char *type;
	time_t time;
	struct bc_list_struct list;
};

static struct bc_event_cam *__alloc_event_cam(int id, bc_event_level_t level,
					      bc_event_cam_type_t type)
{
	struct bc_event_cam *bce = malloc(sizeof(*bce));

	if (bce == NULL)
		return NULL;

	memset(bce, 0, sizeof(*bce));
	bce->id = id;
	bce->level = level_to_str[level];
	bce->type = cam_type_to_str[type];
	bce->start_time = time(NULL);

	return bce;
}

static int bce_open_db(void)
{
	if (bcdb)
		return 0;

	if ((bcdb = bc_db_open()) == NULL)
		return -1;

	return 0;
}

/* When end_time is 0 or not the same as start_time, we add it to the field */
static int __do_cam(struct bc_event_cam *bce)
{
	int res;

	if (bce_open_db())
		return -1;

	if (bce->inserted) {
		if (!bce->end_time)
			bce->end_time = time(NULL);
		return bc_db_query(bcdb, "UPDATE EventsCam (length) VALUES('%lu')",
				   bce->end_time - bce->start_time);
	}

	if (!bce->end_time || bce->end_time != bce->start_time) {
		time_t length = bce->end_time ? bce->end_time - bce->start_time : 0;

		res = bc_db_query(bcdb, "INSERT INTO EventsCam (time,level_id,"
				"device_id,type_id,length) VALUES('%lu','%s','%d',"
				"'%s','%lu')", bce->start_time, bce->level,
				bce->id, bce->type, length);
	} else {
		res = bc_db_query(bcdb, "INSERT INTO EventsCam (time,"
				"level_id,device_id,type_id) VALUES('%lu','%s','%d',"
				"'%s')", bce->start_time, bce->level,
				bce->id, bce->type);
	}

	if (!res)
		bce->inserted = 1;

	return res;
}

static int __do_sys_insert(struct bc_event_sys *bce)
{
	if (bce_open_db())
		return -1;

	return bc_db_query(bcdb, "INSERT INTO EventsSystem (time,level_id,"
                           "type_id) VALUES('%lu','%s','%s')", bce->time,
			   bce->level, bce->type);
}

int bc_event_sys(bc_event_level_t level, bc_event_sys_type_t type)
{
	struct bc_event_sys bce, *bu;

	bce.time = time(NULL);
	bce.level = level_to_str[level];
	bce.type = sys_type_to_str[type];

	if (!__do_sys_insert(&bce))
		return 0;

	bu = malloc(sizeof(bce));
	if (bu == NULL)
		return -1;

	memcpy(bu, &bce, sizeof(bce));

	bc_list_add(&bu->list, &sys_event_queue);

	return 0;
}

bc_event_cam_t bc_event_cam_start(int id, bc_event_level_t level,
				  bc_event_cam_type_t type)
{
	struct bc_event_cam *bce = __alloc_event_cam(id, level, type);

	/* TODO: This failure may need to be directed somewhere */
	if (bce == NULL)
		return BC_EVENT_CAM_FAIL;

	__do_cam(bce);

	return bce;
}

void bc_event_cam_end(bc_event_cam_t bce)
{
	/* On failure, we add it to the event_queue to try later */
	if (__do_cam(bce))
		bc_list_add(&bce->list, &cam_event_queue);
	else
		free(bce);
}

int bc_event_cam(int id, bc_event_level_t level,
		 bc_event_cam_type_t type)
{
	struct bc_event_cam *bce = __alloc_event_cam(id, level, type);

	/* TODO: Failure probably needs to be sent somewhere */
	if (bce == NULL)
		return -1;

	bce->end_time = bce->start_time;

	bc_event_cam_end(bce);

	return 0;
}

/* Run through the queues if needed */
void bc_event_clear(void)
{
	struct bc_event_cam *bcec;
	struct bc_event_sys *bces;
	struct bc_list_struct *lh;

	bc_list_for_each(lh, &cam_event_queue) {
		bcec = bc_list_entry(lh, struct bc_event_cam, list);
		/* On error, do nothing */
		if (__do_cam(bcec))
			continue;

		/* Finally, get this event out of the queue */
		bc_list_del(lh);
		free(bcec);
	}

	bc_list_for_each(lh, &sys_event_queue) {
		bces = bc_list_entry(lh, struct bc_event_sys, list);
		/* On error, do nothing */
		if (__do_sys_insert(bces))
			continue;

		/* Get this one out too */
		bc_list_del(lh);
		free(bces);
	}
}
