/*
 * Copyright (C) 2008 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include "polkitimplicitauthorization.h"
#include "polkittemporaryauthorization.h"
#include "_polkittemporaryauthorization.h"

#include "polkitprivate.h"

/**
 * SECTION:polkittemporaryauthorization
 * @title: PolkitTemporaryAuthorization
 * @short_description: Temporary Authorizations
 *
 * Object used to describe a temporary authorization.
 */

/**
 * PolkitTemporaryAuthorization:
 *
 * The #PolkitTemporaryAuthorization struct should not be accessed directly.
 */
struct _PolkitTemporaryAuthorization
{
  GObject parent_instance;

  _PolkitTemporaryAuthorization *real;

  gchar **annotation_keys;
};

struct _PolkitTemporaryAuthorizationClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (PolkitTemporaryAuthorization, polkit_temporary_authorization, G_TYPE_OBJECT);

static void
polkit_temporary_authorization_init (PolkitTemporaryAuthorization *authorization)
{
}

static void
polkit_temporary_authorization_finalize (GObject *object)
{
  PolkitTemporaryAuthorization *authorization;

  authorization = POLKIT_TEMPORARY_AUTHORIZATION (object);

  g_object_unref (authorization->real);

  if (G_OBJECT_CLASS (polkit_temporary_authorization_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (polkit_temporary_authorization_parent_class)->finalize (object);
}

static void
polkit_temporary_authorization_class_init (PolkitTemporaryAuthorizationClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = polkit_temporary_authorization_finalize;
}

/**
 * polkit_temporary_authorization_new:
 * @id: Id for temporary authorization
 * @action_id: An action id.
 * @subject: A #PolkitSubject.
 * @time_obtained: Time obtained, since the Epoch Jan 1, 1970 0:00 UTC.
 * @time_expires: Time the temporary authorization will expire, since the Epoch Jan 1, 1970 0:00 UTC.
 *
 * Creates a new temporary authorization.
 *
 * Returns: A #PolkitTemporaryAuthorization, free with g_object_unref()
 **/
PolkitTemporaryAuthorization *
polkit_temporary_authorization_new (const gchar                  *id,
                                    const gchar                  *action_id,
                                    PolkitSubject                *subject,
                                    guint64                       time_obtained,
                                    guint64                       time_expires)
{
  PolkitTemporaryAuthorization *ret;
  _PolkitTemporaryAuthorization *real;
  _PolkitSubject *real_subject;

  real_subject = polkit_subject_get_real (subject);
  real = _polkit_temporary_authorization_new (id,
                                              action_id,
                                              real_subject,
                                              time_obtained,
                                              time_expires);
  g_object_unref (real_subject);

  ret = polkit_temporary_authorization_new_for_real (real);
  g_object_unref (real);

  return ret;
}


PolkitTemporaryAuthorization *
polkit_temporary_authorization_new_for_real (_PolkitTemporaryAuthorization *real)
{
  PolkitTemporaryAuthorization *authorization;

  authorization = POLKIT_TEMPORARY_AUTHORIZATION (g_object_new (POLKIT_TYPE_TEMPORARY_AUTHORIZATION, NULL));
  authorization->real = g_object_ref (real);

  return authorization;
}

_PolkitTemporaryAuthorization *
polkit_temporary_authorization_get_real (PolkitTemporaryAuthorization *authorization)
{
  return g_object_ref (authorization->real);
}

/**
 * polkit_temporary_authorization_get_id:
 * @authorization: A #PolkitTemporaryAuthorization.
 *
 * Gets the opaque identifier for @authorization.
 *
 * Returns: A string owned by @authorization. Do not free.
 */
const gchar *
polkit_temporary_authorization_get_id (PolkitTemporaryAuthorization *authorization)
{
  return _polkit_temporary_authorization_get_id (authorization->real);
}

/**
 * polkit_temporary_authorization_get_action_id:
 * @authorization: A #PolkitTemporaryAuthorization.
 *
 * Gets the action that @authorization is for.
 *
 * Returns: A string owned by @authorization. Do not free.
 **/
const gchar *
polkit_temporary_authorization_get_action_id (PolkitTemporaryAuthorization *authorization)
{
  return _polkit_temporary_authorization_get_action_id (authorization->real);
}

/**
 * polkit_temporary_authorization_get_subject:
 * @authorization: A #PolkitTemporaryAuthorization.
 *
 * Gets the subject that @authorization is for.
 *
 * Returns: A #PolkitSubject, free with g_object_unref().
 **/
PolkitSubject *
polkit_temporary_authorization_get_subject (PolkitTemporaryAuthorization *authorization)
{
  return polkit_subject_new_for_real (_polkit_temporary_authorization_get_subject (authorization->real));
}

/**
 * polkit_temporary_authorization_get_time_obtained:
 * @authorization: A #PolkitTemporaryAuthorization.
 *
 * Gets the time when @authorization was obtained.
 *
 * Returns: Seconds since the Epoch Jan 1. 1970, 0:00 UTC.
 **/
guint64
polkit_temporary_authorization_get_time_obtained (PolkitTemporaryAuthorization *authorization)
{
  return _polkit_temporary_authorization_get_time_obtained (authorization->real);
}

/**
 * polkit_temporary_authorization_get_time_expires:
 * @authorization: A #PolkitTemporaryAuthorization.
 *
 * Gets the time when @authorization will expire.
 *
 * Returns: Seconds since the Epoch Jan 1. 1970, 0:00 UTC.
 **/
guint64
polkit_temporary_authorization_get_time_expires (PolkitTemporaryAuthorization *authorization)
{
  return _polkit_temporary_authorization_get_time_expires (authorization->real);
}
