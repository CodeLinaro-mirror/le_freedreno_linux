/*
 * Copyright (C) 2014 Red Hat
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

#include "msm_drv.h"
#include "msm_kms.h"
#include "msm_gem.h"


int msm_atomic_check(struct drm_device *dev, struct drm_atomic_state *a)
{
	return drm_atomic_helper_check(dev, a);
}

int msm_atomic_commit(struct drm_device *dev, struct drm_atomic_state *a,
		bool async)
{
	/* TODO we need to know what fb's have changed and fence those.. also,
	 * drm_atomic_helper_commit() doesn't actually support 'async'.. but I'm
	 * not sure if it should, ie. why not just let driver's _commit() fxn
	 * handle that and then call back drm_atomic_helper_commit() at the
	 * right time..
	 */
	return drm_atomic_helper_commit(dev, a, false);
}
