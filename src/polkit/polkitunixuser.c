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
#include <pwd.h>
#include <errno.h>
#include "polkitunixuser.h"
#include "polkitidentity.h"
#include "polkiterror.h"
#include "polkitprivate.h"

/**
 * SECTION:polkitunixuser
 * @title: PolkitUnixUser
 * @short_description: Unix users
 *
 * An object representing a user identity on a UNIX system.
 */

/**
 * PolkitUnixUser:
 *
 * The #PolkitUnixUser struct should not be accessed directly.
 */
struct _PolkitUnixUser
{
  GObject parent_instance;

  gint uid;
};

struct _PolkitUnixUserClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_0,
  PROP_UID,
};

static void identity_iface_init (PolkitIdentityIface *identity_iface);

G_DEFINE_TYPE_WITH_CODE (PolkitUnixUser, polkit_unix_user, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (POLKIT_TYPE_IDENTITY, identity_iface_init)
                         );

static void
polkit_unix_user_init (PolkitUnixUser *unix_user)
{
}

static void
polkit_unix_user_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  PolkitUnixUser *unix_user = POLKIT_UNIX_USER (object);

  switch (prop_id)
    {
    case PROP_UID:
      g_value_set_int (value, unix_user->uid);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
polkit_unix_user_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  PolkitUnixUser *unix_user = POLKIT_UNIX_USER (object);

  switch (prop_id)
    {
    case PROP_UID:
      unix_user->uid = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
polkit_unix_user_class_init (PolkitUnixUserClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = polkit_unix_user_get_property;
  gobject_class->set_property = polkit_unix_user_set_property;

  /**
   * PolkitUnixUser:uid:
   *
   * The UNIX user id.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_UID,
                                   g_param_spec_int ("uid",
                                                     "User ID",
                                                     "The UNIX user ID",
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_CONSTRUCT |
                                                     G_PARAM_READWRITE |
                                                     G_PARAM_STATIC_NAME |
                                                     G_PARAM_STATIC_BLURB |
                                                     G_PARAM_STATIC_NICK));

}

/**
 * polkit_unix_user_get_uid:
 * @user: A #PolkitUnixUser.
 *
 * Gets the UNIX user id for @user.
 *
 * Returns: A UNIX user id.
 */
gint
polkit_unix_user_get_uid (PolkitUnixUser *user)
{
  return user->uid;
}

/**
 * polkit_unix_user_set_uid:
 * @user: A #PolkitUnixUser.
 * @uid: A UNIX user id.
 *
 * Sets @uid for @user.
 */
void
polkit_unix_user_set_uid (PolkitUnixUser *user,
                          gint uid)
{
  user->uid = uid;
}

/**
 * polkit_unix_user_new:
 * @uid: A UNIX user id.
 *
 * Creates a new #PolkitUnixUser object for @uid.
 *
 * Returns: A #PolkitUnixUser object. Free with g_object_unref().
 */
PolkitIdentity *
polkit_unix_user_new (gint uid)
{
  return POLKIT_IDENTITY (g_object_new (POLKIT_TYPE_UNIX_USER,
                                        "uid", uid,
                                        NULL));
}

/**
 * polkit_unix_user_new_for_name:
 * @name: A UNIX user name.
 * @error: Return location for error.
 *
 * Creates a new #PolkitUnixUser object for a user with the user name
 * @name.
 *
 * Returns: A #PolkitUnixUser object or %NULL if @error is set.
 */
PolkitIdentity *
polkit_unix_user_new_for_name (const gchar    *name,
                               GError        **error)
{
  struct passwd *passwd;
  PolkitIdentity *identity;

  identity = NULL;

  passwd = getpwnam (name);
  if (passwd == NULL)
    {
      g_set_error (error,
                   POLKIT_ERROR,
                   POLKIT_ERROR_FAILED,
                   "No UNIX user with name %s: %s",
                   name,
                   g_strerror (errno));
      goto out;
    }

  identity = polkit_unix_user_new (passwd->pw_uid);

 out:
  return identity;
}

static gboolean
polkit_unix_user_equal (PolkitIdentity *a,
                        PolkitIdentity *b)
{
  PolkitUnixUser *user_a;
  PolkitUnixUser *user_b;

  user_a = POLKIT_UNIX_USER (a);
  user_b = POLKIT_UNIX_USER (b);

  return user_a->uid == user_b->uid;
}

static guint
polkit_unix_user_hash (PolkitIdentity *identity)
{
  PolkitUnixUser *user;

  user = POLKIT_UNIX_USER (identity);

  return g_direct_hash (GINT_TO_POINTER (((gint) (user->uid)) * 2));
}

static gchar *
polkit_unix_user_to_string (PolkitIdentity *identity)
{
  PolkitUnixUser *user = POLKIT_UNIX_USER (identity);
  struct passwd *passwd;

  passwd = getpwuid (user->uid);

  if (passwd == NULL)
    return g_strdup_printf ("unix-user:%d", user->uid);
  else
    return g_strdup_printf ("unix-user:%s", passwd->pw_name);
}

static void
identity_iface_init (PolkitIdentityIface *identity_iface)
{
  identity_iface->hash      = polkit_unix_user_hash;
  identity_iface->equal     = polkit_unix_user_equal;
  identity_iface->to_string = polkit_unix_user_to_string;
}
