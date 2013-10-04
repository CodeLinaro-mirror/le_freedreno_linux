/*
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>

/**
 * drm_atomic_helper_begin - start a sequence of atomic updates
 * @dev: DRM device
 * @flags: the modifier flags that userspace has requested
 *
 * Begin a sequence of atomic property sets.  Returns a driver
 * private state object that is passed back into the various
 * object's set_property() fxns, and into the remainder of the
 * atomic funcs.  The state object should accumulate the changes
 * from one o more set_property()'s.  At the end, the state can
 * be checked, and optionally committed.
 *
 * RETURNS
 *   a driver private state object, which is passed back in to
 *   the various other atomic fxns, or error (such as -EBUSY if
 *   there is still a pending async update)
 */
void *drm_atomic_helper_begin(struct drm_device *dev, uint32_t flags)
{
	struct drm_atomic_helper_state *state;
	int nplanes = dev->mode_config.num_plane;
	int ncrtcs  = dev->mode_config.num_crtc;
	int sz;
	void *ptr;

	sz = sizeof(*state);
	sz += (sizeof(state->planes) + sizeof(state->pstates)) * nplanes;
	sz += (sizeof(state->crtcs) + sizeof(state->cstates)) * ncrtcs;

	ptr = kzalloc(sz, GFP_KERNEL);

	state = ptr;
	ptr = &state[1];

	ww_acquire_init(&state->ww_ctx, &crtc_ww_class);
	INIT_LIST_HEAD(&state->locked_crtcs);

	kref_init(&state->refcount);
	state->dev = dev;
	state->flags = flags;

	state->planes = ptr;
	ptr = &state->planes[nplanes];

	state->pstates = ptr;
	ptr = &state->pstates[nplanes];

	state->crtcs = ptr;
	ptr = &state->crtcs[ncrtcs];

	state->cstates = ptr;
	ptr = &state->cstates[ncrtcs];

	return state;
}
EXPORT_SYMBOL(drm_atomic_helper_begin);

/**
 * drm_atomic_helper_set_event - set a pending event on mode object
 * @dev: DRM device
 * @state: the driver private state object
 * @obj: the object to set the event on
 * @event: the event to send back
 *
 * Set pending event for an update on the specified object.  The
 * event is to be sent back to userspace after the update completes.
 */
int drm_atomic_helper_set_event(struct drm_device *dev,
		void *state, struct drm_mode_object *obj,
		struct drm_pending_vblank_event *event)
{
	switch (obj->type) {
	case DRM_MODE_OBJECT_CRTC: {
		struct drm_crtc_state *cstate =
			drm_atomic_get_crtc_state(obj_to_crtc(obj), state);
		if (IS_ERR(cstate))
			return PTR_ERR(cstate);
		cstate->event = event;
		return 0;
	}
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL(drm_atomic_helper_set_event);

/**
 * drm_atomic_helper_check - validate state object
 * @dev: DRM device
 * @state: the driver private state object
 *
 * Check the state object to see if the requested state is
 * physically possible.
 *
 * RETURNS
 * Zero for success or -errno
 */
int drm_atomic_helper_check(struct drm_device *dev, void *state)
{
	struct drm_atomic_helper_state *a = state;
	int nplanes = dev->mode_config.num_plane;
	int ncrtcs = dev->mode_config.num_crtc;
	int i, ret = 0;

	for (i = 0; i < nplanes; i++) {
		if (a->planes[i]) {
			ret = drm_atomic_check_plane_state(a->planes[i], a->pstates[i]);
			if (ret)
				break;
		}
	}

	for (i = 0; i < ncrtcs; i++) {
		if (a->crtcs[i]) {
			ret = drm_atomic_check_crtc_state(a->crtcs[i], a->cstates[i]);
			if (ret)
				break;
		}
	}

	return ret;
}
EXPORT_SYMBOL(drm_atomic_helper_check);

/* Note that we drop and re-acquire the locks w/ ww_mutex directly,
 * since we keep the crtc in our list with in_atomic == true.
 */

static void drop_locks(struct drm_atomic_helper_state *a,
		struct ww_acquire_ctx *ww_ctx)
{
	struct drm_crtc *crtc;

	mutex_lock(&a->dev->struct_mutex);
	list_for_each_entry(crtc, &a->locked_crtcs, lock_head)
		ww_mutex_unlock(&crtc->mutex);
	mutex_unlock(&a->dev->struct_mutex);

	ww_acquire_fini(ww_ctx);
}

static void grab_locks(struct drm_atomic_helper_state *a,
		struct ww_acquire_ctx *ww_ctx)
{
	struct drm_crtc *crtc = NULL, *slow_locked = NULL, *contended = NULL;
	int ret;


	ww_acquire_init(ww_ctx, &crtc_ww_class);

	/*
	 * We need to do proper rain^Hww dance.. another context
	 * could sneak in a grab the lock in order to check
	 * crtc->in_atomic, and we get -EDEADLK.  But the winner
	 * will realize the mistake when it sees crtc->in_atomic
	 * already set, and then drop lock and return -EBUSY.
	 * So we just need to keep dancing until we win.
	 */
retry:
	ret = 0;
	list_for_each_entry(crtc, &a->locked_crtcs, lock_head) {
		if (crtc == slow_locked) {
			slow_locked = NULL;
			continue;
		}
		contended = crtc;
		ret = ww_mutex_lock(&crtc->mutex, ww_ctx);
		if (ret)
			goto fail;
	}

fail:
	if (ret == -EDEADLK) {
		/* we lost out in a seqno race, backoff, lock and retry.. */

		list_for_each_entry(crtc, &a->locked_crtcs, lock_head) {
			if (crtc == contended)
				break;
			ww_mutex_unlock(&crtc->mutex);
		}

		if (slow_locked)
			ww_mutex_unlock(&slow_locked->mutex);

		ww_mutex_lock_slow(&contended->mutex, ww_ctx);
		slow_locked = contended;
		goto retry;
	}
	WARN_ON(ret);   /* if we get EALREADY then something is fubar */
}

static void commit_locks(struct drm_atomic_helper_state *a,
		struct ww_acquire_ctx *ww_ctx)
{
	struct drm_device *dev = a->dev;
	int nplanes = dev->mode_config.num_plane;
	int ncrtcs = dev->mode_config.num_crtc;
	int i;

	for (i = 0; i < nplanes; i++) {
		struct drm_plane *plane = a->planes[i];
		if (plane) {
			plane->state->state = NULL;
			drm_atomic_helper_destroy_plane_state(plane, a->pstates[i]);
		}
	}

	for (i = 0; i < ncrtcs; i++) {
		struct drm_crtc *crtc = a->crtcs[i];
		if (crtc) {
			crtc->state->state = NULL;
			drm_atomic_helper_destroy_crtc_state(crtc, a->cstates[i]);
		}
	}

	/* and properly release them (clear in_atomic, remove from list): */
	mutex_lock(&dev->struct_mutex);
	while (!list_empty(&a->locked_crtcs)) {
		struct drm_crtc *crtc;

		crtc = list_first_entry(&a->locked_crtcs,
				struct drm_crtc, lock_head);

		drm_modeset_unlock_crtc(crtc);
	}
	mutex_unlock(&dev->struct_mutex);
	ww_acquire_fini(ww_ctx);
	a->committed = true;
}

static int atomic_commit(struct drm_atomic_helper_state *a,
		struct ww_acquire_ctx *ww_ctx)
{
	int nplanes = a->dev->mode_config.num_plane;
	int ncrtcs = a->dev->mode_config.num_crtc;
	int i, ret = 0;

	for (i = 0; i < nplanes; i++) {
		struct drm_plane *plane = a->planes[i];
		if (plane) {
			ret = drm_atomic_commit_plane_state(plane, a->pstates[i]);
			if (ret)
				break;
		}
	}

	for (i = 0; i < ncrtcs; i++) {
		struct drm_crtc *crtc = a->crtcs[i];
		if (crtc) {
			ret = drm_atomic_commit_crtc_state(crtc, a->cstates[i]);
			if (ret)
				break;
		}
	}

	commit_locks(a, ww_ctx);

	return ret;
}

/**
 * drm_atomic_helper_commit - commit state
 * @dev: DRM device
 * @state: the driver private state object
 *
 * Commit the state.  This will only be called if atomic_check()
 * succeeds.
 *
 * RETURNS
 * Zero for success or -errno
 */
int drm_atomic_helper_commit(struct drm_device *dev, void *state)
{
	struct drm_atomic_helper_state *a = state;

	/* this can be either called synchronously from user ioctl
	 * call, or for NONBLOCK updates the driver can defer the
	 * actual commit:
	 */
	if (a->ww_ctx.task != current) {
		struct ww_acquire_ctx ww_ctx;
		grab_locks(a, &ww_ctx);
		return atomic_commit(a, &ww_ctx);
	}
	return atomic_commit(a, &a->ww_ctx);
}
EXPORT_SYMBOL(drm_atomic_helper_commit);

/**
 * drm_atomic_helper_end - conclude the atomic update
 * @dev: DRM device
 * @state: the driver private state object
 *
 * Release resources associated with the state object.
 */
void drm_atomic_helper_end(struct drm_device *dev, void *state)
{
	struct drm_atomic_helper_state *a = state;

	/* if commit is happening from another thread, it will
	 * block grabbing locks until we drop (and not set
	 * a->committed until after), so this is not a race:
	 */
	if (!a->committed)
		drop_locks(a, &a->ww_ctx);

	drm_atomic_helper_state_unreference(state);
}
EXPORT_SYMBOL(drm_atomic_helper_end);

void _drm_atomic_helper_state_free(struct kref *kref)
{
	struct drm_atomic_helper_state *a =
		container_of(kref, struct drm_atomic_helper_state, refcount);

	/* in case we haven't already: */
	if (!a->committed) {
		grab_locks(a, &a->ww_ctx);
		commit_locks(a, &a->ww_ctx);
	}

	kfree(a);
}
EXPORT_SYMBOL(_drm_atomic_helper_state_free);

int drm_atomic_helper_plane_set_property(struct drm_plane *plane, void *state,
		struct drm_property *property, uint64_t val, void *blob_data)
{
	struct drm_plane_state *pstate = drm_atomic_get_plane_state(plane, state);
	if (IS_ERR(pstate))
		return PTR_ERR(pstate);
	return drm_plane_set_property(plane, pstate, property, val, blob_data);
}
EXPORT_SYMBOL(drm_atomic_helper_plane_set_property);

void drm_atomic_helper_init_plane_state(struct drm_plane *plane,
		struct drm_plane_state *pstate, void *state)
{
	/* snapshot current state: */
	*pstate = *plane->state;
	pstate->state = state;
}
EXPORT_SYMBOL(drm_atomic_helper_init_plane_state);

void drm_atomic_helper_destroy_plane_state(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	kfree(state);
}
EXPORT_SYMBOL(drm_atomic_helper_destroy_plane_state);

static struct drm_plane_state *
drm_atomic_helper_get_plane_state(struct drm_plane *plane, void *state)
{
	struct drm_atomic_helper_state *a = state;
	struct drm_plane_state *pstate;
	int ret;

	/* grab lock of current crtc.. if crtc is NULL this grabs all: */
	ret = drm_modeset_lock_crtc(plane->state->crtc, state);
	if (ret)
		return ERR_PTR(ret);

	pstate = a->pstates[plane->id];

	if (!pstate) {
		pstate = kzalloc(sizeof(*pstate), GFP_KERNEL);
		if (!pstate)
			return ERR_PTR(-ENOMEM);
		drm_atomic_helper_init_plane_state(plane, pstate, state);
		a->planes[plane->id] = plane;
		a->pstates[plane->id] = pstate;
	}

	return pstate;
}

static void
swap_plane_state(struct drm_plane *plane, struct drm_atomic_helper_state *a)
{
	struct drm_plane_state *pstate = a->pstates[plane->id];

	/* clear transient state (only valid during atomic update): */
	pstate->new_fb = false;

	swap(plane->state, a->pstates[plane->id]);
	plane->base.propvals = &plane->state->propvals;
}

static int
drm_atomic_helper_commit_plane_state(struct drm_plane *plane,
		struct drm_plane_state *pstate)
{
	struct drm_framebuffer *old_fb = NULL, *fb = NULL;
	int ret = 0;

	fb = pstate->fb;

	if (pstate->crtc && fb) {
		ret = plane->funcs->update_plane(plane, pstate->crtc, pstate->fb,
			pstate->crtc_x, pstate->crtc_y, pstate->crtc_w, pstate->crtc_h,
			pstate->src_x,  pstate->src_y,  pstate->src_w,  pstate->src_h);
		if (!ret) {
			/* on success, update state and fb refcnting: */
			/* NOTE: if we ensure no driver sets plane->state->fb = NULL
			 * on disable, we can move this up a level and not duplicate
			 * nearly the same thing for both update_plane and disable_plane
			 * cases..  I leave it like this for now to be paranoid due to
			 * the slightly different ordering in the two cases in the
			 * original code.
			 */
			old_fb = plane->state->fb;
			swap_plane_state(plane, pstate->state);
			fb = NULL;
		}
	} else {
		old_fb = plane->state->fb;
		plane->funcs->disable_plane(plane);
		swap_plane_state(plane, pstate->state);
	}

	if (fb)
		drm_framebuffer_unreference(fb);
	if (old_fb)
		drm_framebuffer_unreference(old_fb);

	return ret;
}

int drm_atomic_helper_crtc_set_property(struct drm_crtc *crtc, void *state,
		struct drm_property *property, uint64_t val, void *blob_data)
{
	struct drm_crtc_state *cstate = drm_atomic_get_crtc_state(crtc, state);
	if (IS_ERR(cstate))
		return PTR_ERR(cstate);
	return drm_crtc_set_property(crtc, cstate, property, val, blob_data);
}
EXPORT_SYMBOL(drm_atomic_helper_crtc_set_property);

void drm_atomic_helper_init_crtc_state(struct drm_crtc *crtc,
		struct drm_crtc_state *cstate, void *state)
{
	/* snapshot current state: */
	*cstate = *crtc->state;
	cstate->state = state;

	if (cstate->connector_ids) {
		int sz = cstate->num_connector_ids * sizeof(cstate->connector_ids[0]);
		cstate->connector_ids = kmemdup(cstate->connector_ids, sz, GFP_KERNEL);
	}

	/* this should never happen.. but make sure! */
	WARN_ON(cstate->event);
	cstate->event = NULL;
}
EXPORT_SYMBOL(drm_atomic_helper_init_crtc_state);

void drm_atomic_helper_destroy_crtc_state(struct drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	kfree(state->connector_ids);
	kfree(state);
}
EXPORT_SYMBOL(drm_atomic_helper_destroy_crtc_state);

static struct drm_crtc_state *
drm_atomic_helper_get_crtc_state(struct drm_crtc *crtc, void *state)
{
	struct drm_atomic_helper_state *a = state;
	struct drm_crtc_state *cstate;
	int ret;

	ret = drm_modeset_lock_crtc(crtc, state);
	if (ret)
		return ERR_PTR(ret);

	cstate = a->cstates[crtc->id];

	if (!cstate) {
		cstate = kmalloc(sizeof(*cstate), GFP_KERNEL);
		if (!cstate)
			return ERR_PTR(-ENOMEM);
		drm_atomic_helper_init_crtc_state(crtc, cstate, state);
		a->crtcs[crtc->id] = crtc;
		a->cstates[crtc->id] = cstate;
	}
	return cstate;
}

static void
swap_crtc_state(struct drm_crtc *crtc, struct drm_atomic_helper_state *a)
{
	struct drm_crtc_state *cstate = a->cstates[crtc->id];
	struct drm_device *dev = crtc->dev;
	struct drm_pending_vblank_event *event = cstate->event;
	if (event) {
		/* hrm, need to sort out a better way to send events for
		 * other-than-pageflip.. but modeset is not async, so:
		 */
		unsigned long flags;
		spin_lock_irqsave(&dev->event_lock, flags);
		drm_send_vblank_event(dev, crtc->id, event);
		cstate->event = NULL;
		spin_unlock_irqrestore(&dev->event_lock, flags);
	}

	/* clear transient state (only valid during atomic update): */
	cstate->set_config = false;
	cstate->new_fb = false;
	cstate->connectors_change = false;

	swap(crtc->state, a->cstates[crtc->id]);
	crtc->base.propvals = &crtc->state->propvals;
}

static struct drm_connector **get_connector_set(struct drm_device *dev,
		uint32_t *connector_ids, uint32_t num_connector_ids)
{
	struct drm_connector **connector_set = NULL;
	int i;

	connector_set = kmalloc(num_connector_ids *
			sizeof(struct drm_connector *),
			GFP_KERNEL);
	if (!connector_set)
		return NULL;

	for (i = 0; i < num_connector_ids; i++)
		connector_set[i] = drm_connector_find(dev, connector_ids[i]);

	return connector_set;
}

static struct drm_display_mode *get_mode(struct drm_crtc *crtc, struct drm_crtc_state *cstate)
{
	struct drm_display_mode *mode = NULL;
	if (cstate->mode_valid) {
		struct drm_device *dev = crtc->dev;
		int ret;

		mode = drm_mode_create(dev);
		if (!mode)
			return ERR_PTR(-ENOMEM);

		ret = drm_crtc_convert_umode(mode, &cstate->mode);
		if (ret) {
			DRM_DEBUG_KMS("Invalid mode\n");
			drm_mode_destroy(dev, mode);
			return ERR_PTR(ret);
		}

		drm_mode_set_crtcinfo(mode, CRTC_INTERLACE_HALVE_V);
	}
	return mode;
}

static int set_config(struct drm_crtc *crtc, struct drm_crtc_state *cstate)
{
	struct drm_device *dev = crtc->dev;
	struct drm_framebuffer *fb = cstate->fb;
	struct drm_connector **connector_set = get_connector_set(crtc->dev,
			cstate->connector_ids, cstate->num_connector_ids);
	struct drm_display_mode *mode = get_mode(crtc, cstate);
	struct drm_mode_set set = {
			.crtc = crtc,
			.x = cstate->x,
			.y = cstate->y,
			.mode = mode,
			.num_connectors = cstate->num_connector_ids,
			.connectors = connector_set,
			.fb = fb,
	};
	int ret;

	if (IS_ERR(mode)) {
		ret = PTR_ERR(mode);
		return ret;
	}

	ret = drm_mode_set_config_internal(&set);
	if (!ret)
		swap_crtc_state(crtc, cstate->state);

	if (fb)
		drm_framebuffer_unreference(fb);

	kfree(connector_set);
	if (mode)
		drm_mode_destroy(dev, mode);
	return ret;
}

static int
drm_atomic_helper_commit_crtc_state(struct drm_crtc *crtc,
		struct drm_crtc_state *cstate)
{
	struct drm_framebuffer *old_fb = NULL, *fb = NULL;
	struct drm_atomic_helper_state *a = cstate->state;
	int ret = -EINVAL;

	if (cstate->set_config)
		return set_config(crtc, cstate);

	if (cstate->fb) {
		/* pageflip */

		if (crtc->fb == NULL) {
			/* The framebuffer is currently unbound, presumably
			 * due to a hotplug event, that userspace has not
			 * yet discovered.
			 */
			ret = -EBUSY;
			goto out;
		}

		if (crtc->funcs->page_flip == NULL)
			goto out;

		old_fb = crtc->fb;
		fb = cstate->fb;

		ret = crtc->funcs->page_flip(crtc, fb, cstate->event, a->flags);
		if (ret) {
			/* Keep the old fb, don't unref it. */
			old_fb = NULL;
		} else {
			cstate->event = NULL;
			swap_crtc_state(crtc, cstate->state);
			/* Unref only the old framebuffer. */
			fb = NULL;
		}
	} else {
		/* disable */
		struct drm_mode_set set = {
				.crtc = crtc,
				.fb = NULL,
		};

		old_fb = crtc->state->fb;
		ret = drm_mode_set_config_internal(&set);
		if (!ret) {
			swap_crtc_state(crtc, cstate->state);
		}
	}

out:
	if (fb)
		drm_framebuffer_unreference(fb);
	if (old_fb)
		drm_framebuffer_unreference(old_fb);

	return ret;
}

const struct drm_atomic_helper_funcs drm_atomic_helper_funcs = {
		.get_plane_state    = drm_atomic_helper_get_plane_state,
		.check_plane_state  = drm_plane_check_state,
		.commit_plane_state = drm_atomic_helper_commit_plane_state,

		.get_crtc_state     = drm_atomic_helper_get_crtc_state,
		.check_crtc_state   = drm_crtc_check_state,
		.commit_crtc_state  = drm_atomic_helper_commit_crtc_state,
};
EXPORT_SYMBOL(drm_atomic_helper_funcs);
