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

#include "config.h"
#include <errno.h>
#include <pwd.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>

#include <polkit/polkit.h>
#include <polkit/polkitprivate.h>

#include "polkitbackendauthority.h"
#include "polkitbackendactionlookup.h"
#include "polkitbackendlocalauthority.h"

#include "polkitbackendprivate.h"

/**
 * SECTION:polkitbackendauthority
 * @title: PolkitBackendAuthority
 * @short_description: Abstract base class for authority backends
 * @stability: Unstable
 * @see_also: PolkitBackendLocalAuthority
 *
 * To implement an authority backend, simply subclass #PolkitBackendAuthority
 * and implement the required VFuncs.
 */

enum
{
  CHANGED_SIGNAL,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_ABSTRACT_TYPE (PolkitBackendAuthority, polkit_backend_authority, G_TYPE_OBJECT);

static void
polkit_backend_authority_init (PolkitBackendAuthority *local_authority)
{
}

static void
polkit_backend_authority_class_init (PolkitBackendAuthorityClass *klass)
{
  /**
   * PolkitBackendAuthority::changed:
   * @authority: A #PolkitBackendAuthority.
   *
   * Emitted when actions and/or authorizations change.
   */
  signals[CHANGED_SIGNAL] = g_signal_new ("changed",
                                          POLKIT_BACKEND_TYPE_AUTHORITY,
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (PolkitBackendAuthorityClass, changed),
                                          NULL,                   /* accumulator      */
                                          NULL,                   /* accumulator data */
                                          g_cclosure_marshal_VOID__VOID,
                                          G_TYPE_NONE,
                                          0);
}

void
polkit_backend_authority_system_bus_name_owner_changed (PolkitBackendAuthority   *authority,
                                                        const gchar              *name,
                                                        const gchar              *old_owner,
                                                        const gchar              *new_owner)
{
  PolkitBackendAuthorityClass *klass;

  klass = POLKIT_BACKEND_AUTHORITY_GET_CLASS (authority);

  if (klass->system_bus_name_owner_changed != NULL)
    klass->system_bus_name_owner_changed (authority, name, old_owner, new_owner);
}

/**
 * polkit_backend_authority_get_name:
 * @authority: A #PolkitBackendAuthority.
 *
 * Gets the name of the authority backend.
 *
 * Returns: The name of the backend.
 */
const gchar *
polkit_backend_authority_get_name (PolkitBackendAuthority *authority)
{
  PolkitBackendAuthorityClass *klass;
  klass = POLKIT_BACKEND_AUTHORITY_GET_CLASS (authority);
  if (klass->get_name == NULL)
    return "(not set)";
  return klass->get_name (authority);
}

/**
 * polkit_backend_authority_get_version:
 * @authority: A #PolkitBackendAuthority.
 *
 * Gets the version of the authority backend.
 *
 * Returns: The name of the backend.
 */
const gchar *
polkit_backend_authority_get_version (PolkitBackendAuthority *authority)
{
  PolkitBackendAuthorityClass *klass;
  klass = POLKIT_BACKEND_AUTHORITY_GET_CLASS (authority);
  if (klass->get_version == NULL)
    return "(not set)";
  return klass->get_version (authority);
}

/**
 * polkit_backend_authority_get_features:
 * @authority: A #PolkitBackendAuthority.
 *
 * Gets the features supported by the authority backend.
 *
 * Returns: Flags from #PolkitAuthorityFeatures.
 */
PolkitAuthorityFeatures
polkit_backend_authority_get_features (PolkitBackendAuthority *authority)
{
  PolkitBackendAuthorityClass *klass;
  klass = POLKIT_BACKEND_AUTHORITY_GET_CLASS (authority);
  if (klass->get_features == NULL)
    return POLKIT_AUTHORITY_FEATURES_NONE;
  return klass->get_features (authority);
}

/**
 * polkit_backend_authority_enumerate_actions:
 * @authority: A #PolkitBackendAuthority.
 * @caller: The system bus name that initiated the query.
 * @locale: The locale to retrieve descriptions for.
 * @error: Return location for error or %NULL.
 *
 * Retrieves all registered actions.
 *
 * Returns: A list of #PolkitActionDescription objects or %NULL if @error is set. The returned list
 * should be freed with g_list_free() after each element have been freed with g_object_unref().
 **/
GList *
polkit_backend_authority_enumerate_actions (PolkitBackendAuthority   *authority,
                                            PolkitSubject            *caller,
                                            const gchar              *locale,
                                            GError                  **error)
{
  PolkitBackendAuthorityClass *klass;

  klass = POLKIT_BACKEND_AUTHORITY_GET_CLASS (authority);

  if (klass->enumerate_actions == NULL)
    {
      g_warning ("enumerate_actions is not implemented (it is not optional)");
      g_set_error (error,
                   POLKIT_ERROR,
                   POLKIT_ERROR_NOT_SUPPORTED,
                   "Operation not supported (bug in backend)");
      return NULL;
    }
  else
    {
      return klass->enumerate_actions (authority, caller, locale, error);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * polkit_backend_authority_check_authorization:
 * @authority: A #PolkitBackendAuthority.
 * @caller: The system bus name that initiated the query.
 * @subject: A #PolkitSubject.
 * @action_id: The action to check for.
 * @details: Details about the action or %NULL.
 * @flags: A set of #PolkitCheckAuthorizationFlags.
 * @cancellable: A #GCancellable.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied.
 * @user_data: The data to pass to @callback.
 *
 * Asynchronously checks if @subject is authorized to perform the action represented
 * by @action_id.
 *
 * When the operation is finished, @callback will be invoked. You can then
 * call polkit_backend_authority_check_authorization_finish() to get the result of
 * the operation.
 **/
void
polkit_backend_authority_check_authorization (PolkitBackendAuthority        *authority,
                                              PolkitSubject                 *caller,
                                              PolkitSubject                 *subject,
                                              const gchar                   *action_id,
                                              PolkitDetails                 *details,
                                              PolkitCheckAuthorizationFlags  flags,
                                              GCancellable                  *cancellable,
                                              GAsyncReadyCallback            callback,
                                              gpointer                       user_data)
{
  PolkitBackendAuthorityClass *klass;

  klass = POLKIT_BACKEND_AUTHORITY_GET_CLASS (authority);

  if (klass->check_authorization == NULL)
    {
      GSimpleAsyncResult *simple;

      g_warning ("check_authorization is not implemented (it is not optional)");

      simple = g_simple_async_result_new_error (G_OBJECT (authority),
                                                callback,
                                                user_data,
                                                POLKIT_ERROR,
                                                POLKIT_ERROR_NOT_SUPPORTED,
                                                "Operation not supported (bug in backend)");
      g_simple_async_result_complete (simple);
      g_object_unref (simple);
    }
  else
    {
      klass->check_authorization (authority, caller, subject, action_id, details, flags, cancellable, callback, user_data);
    }
}

/**
 * polkit_backend_authority_check_authorization_finish:
 * @authority: A #PolkitBackendAuthority.
 * @res: A #GAsyncResult obtained from the callback.
 * @error: Return location for error or %NULL.
 *
 * Finishes checking if a subject is authorized for an action.
 *
 * Returns: A #PolkitAuthorizationResult or %NULL if @error is set. Free with g_object_unref().
 **/
PolkitAuthorizationResult *
polkit_backend_authority_check_authorization_finish (PolkitBackendAuthority  *authority,
                                                     GAsyncResult            *res,
                                                     GError                 **error)
{
  PolkitBackendAuthorityClass *klass;

  klass = POLKIT_BACKEND_AUTHORITY_GET_CLASS (authority);

  if (klass->check_authorization_finish == NULL)
    {
      g_warning ("check_authorization_finish is not implemented (it is not optional)");
      g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
      return NULL;
    }
  else
    {
      return klass->check_authorization_finish (authority, res, error);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * polkit_backend_authority_register_authentication_agent:
 * @authority: A #PolkitBackendAuthority.
 * @caller: The system bus name that initiated the query.
 * @subject: The subject the authentication agent wants to register for.
 * @locale: The locale of the authentication agent.
 * @object_path: The object path for the authentication agent.
 * @error: Return location for error or %NULL.
 *
 * Registers an authentication agent.
 *
 * Returns: %TRUE if the authentication agent was successfully registered, %FALSE if @error is set.
 **/
gboolean
polkit_backend_authority_register_authentication_agent (PolkitBackendAuthority    *authority,
                                                        PolkitSubject             *caller,
                                                        PolkitSubject             *subject,
                                                        const gchar               *locale,
                                                        const gchar               *object_path,
                                                        GError                   **error)
{
  PolkitBackendAuthorityClass *klass;

  klass = POLKIT_BACKEND_AUTHORITY_GET_CLASS (authority);

  if (klass->register_authentication_agent == NULL)
    {
      g_set_error (error,
                   POLKIT_ERROR,
                   POLKIT_ERROR_NOT_SUPPORTED,
                   "Operation not supported");
      return FALSE;
    }
  else
    {
      return klass->register_authentication_agent (authority, caller, subject, locale, object_path, error);
    }
}

/**
 * polkit_backend_authority_unregister_authentication_agent:
 * @authority: A #PolkitBackendAuthority.
 * @caller: The system bus name that initiated the query.
 * @subject: The subject the agent claims to be registered at.
 * @object_path: The object path that the authentication agent is registered at.
 * @error: Return location for error or %NULL.
 *
 * Unregisters an authentication agent.
 *
 * Returns: %TRUE if the authentication agent was successfully unregistered, %FALSE if @error is set.
 **/
gboolean
polkit_backend_authority_unregister_authentication_agent (PolkitBackendAuthority    *authority,
                                                          PolkitSubject             *caller,
                                                          PolkitSubject             *subject,
                                                          const gchar               *object_path,
                                                          GError                   **error)
{
  PolkitBackendAuthorityClass *klass;

  klass = POLKIT_BACKEND_AUTHORITY_GET_CLASS (authority);

  if (klass->unregister_authentication_agent == NULL)
    {
      g_set_error (error,
                   POLKIT_ERROR,
                   POLKIT_ERROR_NOT_SUPPORTED,
                   "Operation not supported");
      return FALSE;
    }
  else
    {
      return klass->unregister_authentication_agent (authority, caller, subject, object_path, error);
    }
}

/**
 * polkit_backend_authority_authentication_agent_response:
 * @authority: A #PolkitBackendAuthority.
 * @caller: The system bus name that initiated the query.
 * @cookie: The cookie passed to the authentication agent from the authority.
 * @identity: The identity that was authenticated.
 * @error: Return location for error or %NULL.
 *
 * Provide response that @identity successfully authenticated for the
 * authentication request identified by @cookie.
 *
 * Returns: %TRUE if @authority acknowledged the call, %FALSE if @error is set.
 **/
gboolean
polkit_backend_authority_authentication_agent_response (PolkitBackendAuthority    *authority,
                                                        PolkitSubject             *caller,
                                                        const gchar               *cookie,
                                                        PolkitIdentity            *identity,
                                                        GError                   **error)
{
  PolkitBackendAuthorityClass *klass;

  klass = POLKIT_BACKEND_AUTHORITY_GET_CLASS (authority);

  if (klass->authentication_agent_response == NULL)
    {
      g_set_error (error,
                   POLKIT_ERROR,
                   POLKIT_ERROR_NOT_SUPPORTED,
                   "Operation not supported");
      return FALSE;
    }
  else
    {
      return klass->authentication_agent_response (authority, caller, cookie, identity, error);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * polkit_backend_authority_enumerate_temporary_authorizations:
 * @authority: A #PolkitBackendAuthority.
 * @caller: The system bus name that initiated the query.
 * @subject: The subject to get temporary authorizations for.
 * @error: Return location for error.
 *
 * Gets temporary authorizations for @subject.
 *
 * Returns: A list of #PolkitTemporaryAuthorization objects or %NULL if @error is set. The returned list
 * should be freed with g_list_free() after each element have been freed with g_object_unref().
 */
GList *
polkit_backend_authority_enumerate_temporary_authorizations (PolkitBackendAuthority   *authority,
                                                             PolkitSubject            *caller,
                                                             PolkitSubject            *subject,
                                                             GError                  **error)
{
  PolkitBackendAuthorityClass *klass;

  klass = POLKIT_BACKEND_AUTHORITY_GET_CLASS (authority);

  if (klass->enumerate_temporary_authorizations == NULL)
    {
      g_set_error (error,
                   POLKIT_ERROR,
                   POLKIT_ERROR_NOT_SUPPORTED,
                   "Operation not supported");
      return NULL;
    }
  else
    {
      return klass->enumerate_temporary_authorizations (authority, caller, subject, error);
    }
}

/**
 * polkit_backend_authority_revoke_temporary_authorizations:
 * @authority: A #PolkitBackendAuthority.
 * @caller: The system bus name that initiated the query.
 * @subject: The subject to revoke temporary authorizations for.
 * @error: Return location for error.
 *
 * Revokes temporary authorizations for @subject.
 *
 * Returns: %TRUE if the operation succeeded, %FALSE if @error is set.
 **/
gboolean
polkit_backend_authority_revoke_temporary_authorizations (PolkitBackendAuthority   *authority,
                                                          PolkitSubject            *caller,
                                                          PolkitSubject            *subject,
                                                          GError                  **error)
{
  PolkitBackendAuthorityClass *klass;

  klass = POLKIT_BACKEND_AUTHORITY_GET_CLASS (authority);

  if (klass->revoke_temporary_authorizations == NULL)
    {
      g_set_error (error,
                   POLKIT_ERROR,
                   POLKIT_ERROR_NOT_SUPPORTED,
                   "Operation not supported");
      return FALSE;
    }
  else
    {
      return klass->revoke_temporary_authorizations (authority, caller, subject, error);
    }
}

/**
 * polkit_backend_authority_revoke_temporary_authorization_by_id:
 * @authority: A #PolkitBackendAuthority.
 * @caller: The system bus name that initiated the query.
 * @id: The opaque identifier of the temporary authorization.
 * @error: Return location for error.
 *
 * Revokes a temporary authorizations with opaque identifier @id.
 *
 * Returns: %TRUE if the operation succeeded, %FALSE if @error is set.
 **/
gboolean
polkit_backend_authority_revoke_temporary_authorization_by_id (PolkitBackendAuthority   *authority,
                                                               PolkitSubject            *caller,
                                                               const gchar              *id,
                                                               GError                  **error)
{
  PolkitBackendAuthorityClass *klass;

  klass = POLKIT_BACKEND_AUTHORITY_GET_CLASS (authority);

  if (klass->revoke_temporary_authorization_by_id == NULL)
    {
      g_set_error (error,
                   POLKIT_ERROR,
                   POLKIT_ERROR_NOT_SUPPORTED,
                   "Operation not supported");
      return FALSE;
    }
  else
    {
      return klass->revoke_temporary_authorization_by_id (authority, caller, id, error);
    }
}

/**
 * polkit_backend_authority_add_lockdown_for_action:
 * @authority: A #PolkitBackendAuthority.
 * @caller: The system bus name that called the method.
 * @action_id: The action id.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied.
 * @user_data: The data to pass to @callback.
 *
 * Asynchronously add locks down for @action_id.
 *
 * When the operation is finished, @callback will be invoked. You can
 * then call polkit_backend_authority_add_lockdown_for_action_finish()
 * to get the result of the operation.
 */
void
polkit_backend_authority_add_lockdown_for_action (PolkitBackendAuthority  *authority,
                                                  PolkitSubject           *caller,
                                                  const gchar             *action_id,
                                                  GAsyncReadyCallback      callback,
                                                  gpointer                 user_data)
{
  PolkitBackendAuthorityClass *klass;

  klass = POLKIT_BACKEND_AUTHORITY_GET_CLASS (authority);

  if (klass->add_lockdown_for_action == NULL)
    {
      GSimpleAsyncResult *simple;

      simple = g_simple_async_result_new_error (G_OBJECT (authority),
                                                callback,
                                                user_data,
                                                POLKIT_ERROR,
                                                POLKIT_ERROR_NOT_SUPPORTED,
                                                "Operation not supported");
      g_simple_async_result_complete (simple);
      g_object_unref (simple);
    }
  else
    {
      klass->add_lockdown_for_action (authority, caller, action_id, callback, user_data);
    }
}

/**
 * polkit_backend_authority_add_lockdown_for_action_finish:
 * @authority: A #PolkitBackendAuthority.
 * @res: A #GAsyncResult obtained from the callback.
 * @error: Return location for error or %NULL.
 *
 * Finishes adding lock down for an action.
 *
 * Returns: %TRUE if the operation succeeded or, %FALE if @error is set.
 */
gboolean
polkit_backend_authority_add_lockdown_for_action_finish (PolkitBackendAuthority  *authority,
                                                         GAsyncResult            *res,
                                                         GError                 **error)
{
  PolkitBackendAuthorityClass *klass;

  klass = POLKIT_BACKEND_AUTHORITY_GET_CLASS (authority);

  if (klass->add_lockdown_for_action_finish == NULL)
    {
      g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
      return FALSE;
    }
  else
    {
      return klass->add_lockdown_for_action_finish (authority, res, error);
    }
}

/**
 * polkit_backend_authority_remove_lockdown_for_action:
 * @authority: A #PolkitBackendAuthority.
 * @caller: The system bus name that called the method.
 * @action_id: The action id.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied.
 * @user_data: The data to pass to @callback.
 *
 * Asynchronously remove locks down for @action_id.
 *
 * When the operation is finished, @callback will be invoked. You can
 * then call polkit_backend_authority_remove_lockdown_for_action_finish()
 * to get the result of the operation.
 */
void
polkit_backend_authority_remove_lockdown_for_action (PolkitBackendAuthority  *authority,
                                                     PolkitSubject           *caller,
                                                     const gchar             *action_id,
                                                     GAsyncReadyCallback      callback,
                                                     gpointer                 user_data)
{
  PolkitBackendAuthorityClass *klass;

  klass = POLKIT_BACKEND_AUTHORITY_GET_CLASS (authority);

  if (klass->remove_lockdown_for_action == NULL)
    {
      GSimpleAsyncResult *simple;

      simple = g_simple_async_result_new_error (G_OBJECT (authority),
                                                callback,
                                                user_data,
                                                POLKIT_ERROR,
                                                POLKIT_ERROR_NOT_SUPPORTED,
                                                "Operation not supported");
      g_simple_async_result_complete (simple);
      g_object_unref (simple);
    }
  else
    {
      klass->remove_lockdown_for_action (authority, caller, action_id, callback, user_data);
    }
}

/**
 * polkit_backend_authority_remove_lockdown_for_action_finish:
 * @authority: A #PolkitBackendAuthority.
 * @res: A #GAsyncResult obtained from the callback.
 * @error: Return location for error or %NULL.
 *
 * Finishes removing lock down for an action.
 *
 * Returns: %TRUE if the operation succeeded or, %FALE if @error is set.
 */
gboolean
polkit_backend_authority_remove_lockdown_for_action_finish (PolkitBackendAuthority  *authority,
                                                            GAsyncResult            *res,
                                                            GError                 **error)
{
  PolkitBackendAuthorityClass *klass;

  klass = POLKIT_BACKEND_AUTHORITY_GET_CLASS (authority);

  if (klass->remove_lockdown_for_action_finish == NULL)
    {
      g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error);
      return FALSE;
    }
  else
    {
      return klass->remove_lockdown_for_action_finish (authority, res, error);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

#define TYPE_SERVER         (server_get_type ())
#define SERVER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_SERVER, Server))
#define SERVER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), TYPE_SERVER, ServerClass))
#define SERVER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_SERVER,ServerClass))
#define IS_SERVER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TYPE_SERVER))
#define IS_SERVER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TYPE_SERVER))

typedef struct _Server Server;
typedef struct _ServerClass ServerClass;

GType server_get_type (void) G_GNUC_CONST;

struct _Server
{
  GObject parent_instance;

  PolkitBackendAuthority *authority;

  EggDBusConnection *system_bus;

  EggDBusObjectProxy *bus_proxy;

  EggDBusBus *bus;

  gulong name_owner_changed_id;

  gulong authority_changed_id;

  gchar *well_known_name;

  GHashTable *cancellation_id_to_cancellable;
};

struct _ServerClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_0,
  PROP_BACKEND_NAME,
  PROP_BACKEND_VERSION,
  PROP_BACKEND_FEATURES
};

static void authority_iface_init         (_PolkitAuthorityIface        *authority_iface);

G_DEFINE_TYPE_WITH_CODE (Server, server, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (_POLKIT_TYPE_AUTHORITY, authority_iface_init)
                         );

static void
server_init (Server *server)
{
  server->cancellation_id_to_cancellable = g_hash_table_new_full (g_str_hash,
                                                                  g_str_equal,
                                                                  g_free,
                                                                  g_object_unref);
}

static void
server_finalize (GObject *object)
{
  Server *server;

  server = SERVER (object);

  g_free (server->well_known_name);

  /* TODO: release well_known_name if not NULL */

  g_signal_handler_disconnect (server->bus, server->name_owner_changed_id);

  g_object_unref (server->bus_proxy);

  g_object_unref (server->system_bus);

  g_signal_handler_disconnect (server->authority, server->authority_changed_id);

  g_hash_table_unref (server->cancellation_id_to_cancellable);
}

static void
server_get_property (GObject    *object,
                     guint       prop_id,
                     GValue     *value,
                     GParamSpec *pspec)
{
  Server *server = SERVER (object);

  switch (prop_id)
    {
    case PROP_BACKEND_NAME:
      g_value_set_string (value, polkit_backend_authority_get_name (server->authority));
      break;

    case PROP_BACKEND_VERSION:
      g_value_set_string (value, polkit_backend_authority_get_version (server->authority));
      break;

    case PROP_BACKEND_FEATURES:
      g_value_set_flags (value, polkit_backend_authority_get_features (server->authority));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
server_class_init (ServerClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = server_finalize;
  gobject_class->get_property = server_get_property;

  g_assert (_polkit_authority_override_properties (gobject_class, PROP_BACKEND_NAME) == PROP_BACKEND_FEATURES);
}

static void
name_owner_changed (EggDBusBus *instance,
                    gchar      *name,
                    gchar      *old_owner,
                    gchar      *new_owner,
                    Server     *server)
{
  polkit_backend_authority_system_bus_name_owner_changed (server->authority, name, old_owner, new_owner);
}

static void
authority_changed (PolkitBackendAuthority *authority,
                   Server                 *server)
{
  _polkit_authority_emit_signal_changed (_POLKIT_AUTHORITY (server), NULL);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
authority_handle_enumerate_actions (_PolkitAuthority        *instance,
                                    const gchar             *locale,
                                    EggDBusMethodInvocation *method_invocation)
{
  Server *server = SERVER (instance);
  PolkitSubject *caller;
  EggDBusArraySeq *array;
  GError *error;
  GList *actions;
  GList *l;

  error = NULL;
  caller = NULL;
  actions = NULL;

  caller = polkit_system_bus_name_new (egg_dbus_method_invocation_get_caller (method_invocation));

  actions = polkit_backend_authority_enumerate_actions (server->authority,
                                                        caller,
                                                        locale,
                                                        &error);
  if (error != NULL)
    {
      egg_dbus_method_invocation_return_gerror (method_invocation, error);
      g_error_free (error);
      goto out;
    }

  array = egg_dbus_array_seq_new (G_TYPE_OBJECT, //_POLKIT_TYPE_ACTION_DESCRIPTION,
                                  (GDestroyNotify) g_object_unref,
                                  NULL,
                                  NULL);

  for (l = actions; l != NULL; l = l->next)
    {
      PolkitActionDescription *ad = POLKIT_ACTION_DESCRIPTION (l->data);
      _PolkitActionDescription *real;

      real = polkit_action_description_get_real (ad);
      egg_dbus_array_seq_add (array, real);
    }

  _polkit_authority_handle_enumerate_actions_finish (method_invocation, array);

  g_object_unref (array);

 out:
  g_list_foreach (actions, (GFunc) g_object_unref, NULL);
  g_list_free (actions);
  g_object_unref (caller);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
check_auth_cb (GObject      *source_object,
               GAsyncResult *res,
               gpointer      user_data)
{
  EggDBusMethodInvocation *method_invocation = EGG_DBUS_METHOD_INVOCATION (user_data);
  const gchar *full_cancellation_id;
  PolkitAuthorizationResult *result;
  GError *error;

  error = NULL;
  result = polkit_backend_authority_check_authorization_finish (POLKIT_BACKEND_AUTHORITY (source_object),
                                                                res,
                                                                &error);

  full_cancellation_id = g_object_get_data (G_OBJECT (method_invocation), "cancellation-id");
  if (full_cancellation_id != NULL)
    {
      Server *server;
      server = SERVER (g_object_get_data (G_OBJECT (method_invocation), "server"));
      g_hash_table_remove (server->cancellation_id_to_cancellable, full_cancellation_id);
    }

  if (error != NULL)
    {
      egg_dbus_method_invocation_return_gerror (method_invocation, error);
      g_error_free (error);
    }
  else
    {
      _PolkitAuthorizationResult *real_result;
      real_result = polkit_authorization_result_get_real (result);
      _polkit_authority_handle_check_authorization_finish (method_invocation, real_result);
      g_object_unref (real_result);
      g_object_unref (result);
    }
}

static void
authority_handle_check_authorization (_PolkitAuthority               *instance,
                                      _PolkitSubject                 *real_subject,
                                      const gchar                    *action_id,
                                      EggDBusHashMap                 *real_details,
                                      _PolkitCheckAuthorizationFlags  flags,
                                      const gchar                    *cancellation_id,
                                      EggDBusMethodInvocation        *method_invocation)
{
  Server *server = SERVER (instance);
  const gchar *caller_name;
  PolkitSubject *subject;
  PolkitSubject *caller;
  GCancellable *cancellable;
  PolkitDetails *details;

  details = NULL;

  subject = polkit_subject_new_for_real (real_subject);
  if (subject == NULL)
    {
      egg_dbus_method_invocation_return_error_literal (method_invocation,
                                                       _POLKIT_ERROR,
                                                       _POLKIT_ERROR_FAILED,
                                                       "Error parsing subject struct");
      goto out;
    }

  caller_name = egg_dbus_method_invocation_get_caller (method_invocation);
  caller = polkit_system_bus_name_new (caller_name);

  details = polkit_details_new_for_hash (real_details->data);

  g_object_set_data_full (G_OBJECT (method_invocation), "caller", caller, (GDestroyNotify) g_object_unref);
  g_object_set_data_full (G_OBJECT (method_invocation), "subject", subject, (GDestroyNotify) g_object_unref);

  cancellable = NULL;
  if (cancellation_id != NULL && strlen (cancellation_id) > 0)
    {
      gchar *full_cancellation_id;

      full_cancellation_id = g_strdup_printf ("%s-%s", caller_name, cancellation_id);

      if (g_hash_table_lookup (server->cancellation_id_to_cancellable, full_cancellation_id) != NULL)
        {
          egg_dbus_method_invocation_return_error (method_invocation,
                                                   _POLKIT_ERROR,
                                                   _POLKIT_ERROR_CANCELLATION_ID_NOT_UNIQUE,
                                                   "Given cancellation_id %s is already in use for name %s",
                                                   cancellation_id,
                                                   caller_name);
          g_free (full_cancellation_id);
          goto out;
        }

      cancellable = g_cancellable_new ();

      g_hash_table_insert (server->cancellation_id_to_cancellable,
                           full_cancellation_id,
                           cancellable);

      g_object_set_data (G_OBJECT (method_invocation), "server", server);
      g_object_set_data (G_OBJECT (method_invocation), "cancellation-id", full_cancellation_id);
    }

  polkit_backend_authority_check_authorization (server->authority,
                                                caller,
                                                subject,
                                                action_id,
                                                details,
                                                flags,
                                                cancellable,
                                                check_auth_cb,
                                                method_invocation);
 out:
  if (details != NULL)
    g_object_unref (details);
}

static void
authority_handle_cancel_check_authorization (_PolkitAuthority               *instance,
                                             const gchar                    *cancellation_id,
                                             EggDBusMethodInvocation        *method_invocation)
{
  Server *server = SERVER (instance);
  GCancellable *cancellable;
  const gchar *caller_name;
  gchar *full_cancellation_id;

  caller_name = egg_dbus_method_invocation_get_caller (method_invocation);

  full_cancellation_id = g_strdup_printf ("%s-%s", caller_name, cancellation_id);

  cancellable = g_hash_table_lookup (server->cancellation_id_to_cancellable, full_cancellation_id);
  if (cancellable == NULL)
    {
      egg_dbus_method_invocation_return_error (method_invocation,
                                               _POLKIT_ERROR,
                                               _POLKIT_ERROR_FAILED,
                                               "No such cancellation_id %s for name %s",
                                               cancellation_id,
                                               caller_name);
      goto out;
    }

  g_cancellable_cancel (cancellable);

  _polkit_authority_handle_cancel_check_authorization_finish (method_invocation);

 out:
  g_free (full_cancellation_id);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
authority_handle_register_authentication_agent (_PolkitAuthority               *instance,
                                                _PolkitSubject                 *real_subject,
                                                const gchar                    *locale,
                                                const gchar                    *object_path,
                                                EggDBusMethodInvocation        *method_invocation)
{
  Server *server = SERVER (instance);
  PolkitSubject *caller;
  PolkitSubject *subject;
  GError *error;

  caller = NULL;

  subject = polkit_subject_new_for_real (real_subject);
  if (subject == NULL)
    {
      egg_dbus_method_invocation_return_error_literal (method_invocation,
                                                       _POLKIT_ERROR,
                                                       _POLKIT_ERROR_FAILED,
                                                       "Error parsing subject struct");
      goto out;
    }
  g_object_set_data_full (G_OBJECT (method_invocation), "subject", subject, (GDestroyNotify) g_object_unref);

  caller = polkit_system_bus_name_new (egg_dbus_method_invocation_get_caller (method_invocation));

  error = NULL;
  if (!polkit_backend_authority_register_authentication_agent (server->authority,
                                                               caller,
                                                               subject,
                                                               locale,
                                                               object_path,
                                                               &error))
    {
      egg_dbus_method_invocation_return_gerror (method_invocation, error);
      g_error_free (error);
      goto out;
    }

  _polkit_authority_handle_register_authentication_agent_finish (method_invocation);

 out:
  if (caller != NULL)
    g_object_unref (caller);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
authority_handle_unregister_authentication_agent (_PolkitAuthority               *instance,
                                                  _PolkitSubject                 *real_subject,
                                                  const gchar                    *object_path,
                                                  EggDBusMethodInvocation        *method_invocation)
{
  Server *server = SERVER (instance);
  PolkitSubject *caller;
  PolkitSubject *subject;
  GError *error;

  caller = NULL;

  subject = polkit_subject_new_for_real (real_subject);
  if (subject == NULL)
    {
      egg_dbus_method_invocation_return_error_literal (method_invocation,
                                                       _POLKIT_ERROR,
                                                       _POLKIT_ERROR_FAILED,
                                                       "Error parsing subject struct");
      goto out;
    }
  g_object_set_data_full (G_OBJECT (method_invocation), "subject", subject, (GDestroyNotify) g_object_unref);

  caller = polkit_system_bus_name_new (egg_dbus_method_invocation_get_caller (method_invocation));

  error = NULL;
  if (!polkit_backend_authority_unregister_authentication_agent (server->authority,
                                                                 caller,
                                                                 subject,
                                                                 object_path,
                                                                 &error))
    {
      egg_dbus_method_invocation_return_gerror (method_invocation, error);
      g_error_free (error);
      goto out;
    }

  _polkit_authority_handle_unregister_authentication_agent_finish (method_invocation);

 out:
  if (caller != NULL)
    g_object_unref (caller);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
authority_handle_authentication_agent_response (_PolkitAuthority               *instance,
                                                const gchar                    *cookie,
                                                _PolkitIdentity                *real_identity,
                                                EggDBusMethodInvocation        *method_invocation)
{
  Server *server = SERVER (instance);
  PolkitSubject *caller;
  PolkitIdentity *identity;
  GError *error;

  caller = NULL;
  identity = NULL;

  identity = polkit_identity_new_for_real (real_identity);
  if (identity == NULL)
    {
      egg_dbus_method_invocation_return_error_literal (method_invocation,
                                                       _POLKIT_ERROR,
                                                       _POLKIT_ERROR_FAILED,
                                                       "Error parsing identity struct");
      goto out;
    }

  caller = polkit_system_bus_name_new (egg_dbus_method_invocation_get_caller (method_invocation));

  error = NULL;
  if (!polkit_backend_authority_authentication_agent_response (server->authority,
                                                               caller,
                                                               cookie,
                                                               identity,
                                                               &error))
    {
      egg_dbus_method_invocation_return_gerror (method_invocation, error);
      g_error_free (error);
      goto out;
    }

  _polkit_authority_handle_authentication_agent_response_finish (method_invocation);

 out:
  if (caller != NULL)
    g_object_unref (caller);

  if (identity != NULL)
    g_object_unref (identity);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
authority_handle_enumerate_temporary_authorizations (_PolkitAuthority        *instance,
                                                     _PolkitSubject          *real_subject,
                                                     EggDBusMethodInvocation *method_invocation)
{
  Server *server = SERVER (instance);
  PolkitSubject *caller;
  EggDBusArraySeq *array;
  GError *error;
  GList *temporary_authorizations;
  GList *l;
  PolkitSubject *subject;

  error = NULL;
  caller = NULL;
  temporary_authorizations = NULL;

  subject = polkit_subject_new_for_real (real_subject);
  if (subject == NULL)
    {
      egg_dbus_method_invocation_return_error_literal (method_invocation,
                                                       _POLKIT_ERROR,
                                                       _POLKIT_ERROR_FAILED,
                                                       "Error parsing subject struct");
      goto out;
    }
  g_object_set_data_full (G_OBJECT (method_invocation), "subject", subject, (GDestroyNotify) g_object_unref);

  caller = polkit_system_bus_name_new (egg_dbus_method_invocation_get_caller (method_invocation));

  temporary_authorizations = polkit_backend_authority_enumerate_temporary_authorizations (server->authority,
                                                                                          caller,
                                                                                          subject,
                                                                                          &error);
  if (error != NULL)
    {
      egg_dbus_method_invocation_return_gerror (method_invocation, error);
      g_error_free (error);
      goto out;
    }

  array = egg_dbus_array_seq_new (G_TYPE_OBJECT, //_POLKIT_TYPE_TEMPORARY_AUTHORIZATION,
                                  (GDestroyNotify) g_object_unref,
                                  NULL,
                                  NULL);

  for (l = temporary_authorizations; l != NULL; l = l->next)
    {
      PolkitTemporaryAuthorization *ta = POLKIT_TEMPORARY_AUTHORIZATION (l->data);
      _PolkitTemporaryAuthorization *real;

      real = polkit_temporary_authorization_get_real (ta);
      egg_dbus_array_seq_add (array, real);
    }

  _polkit_authority_handle_enumerate_temporary_authorizations_finish (method_invocation, array);

  g_object_unref (array);

 out:
  g_list_foreach (temporary_authorizations, (GFunc) g_object_unref, NULL);
  g_list_free (temporary_authorizations);
  if (caller != NULL)
    g_object_unref (caller);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
authority_handle_revoke_temporary_authorizations (_PolkitAuthority        *instance,
                                                  _PolkitSubject          *real_subject,
                                                  EggDBusMethodInvocation *method_invocation)
{
  Server *server = SERVER (instance);
  PolkitSubject *caller;
  GError *error;
  PolkitSubject *subject;

  error = NULL;
  caller = NULL;

  subject = polkit_subject_new_for_real (real_subject);
  if (subject == NULL)
    {
      egg_dbus_method_invocation_return_error_literal (method_invocation,
                                                       _POLKIT_ERROR,
                                                       _POLKIT_ERROR_FAILED,
                                                       "Error parsing subject struct");
      goto out;
    }
  g_object_set_data_full (G_OBJECT (method_invocation), "subject", subject, (GDestroyNotify) g_object_unref);

  caller = polkit_system_bus_name_new (egg_dbus_method_invocation_get_caller (method_invocation));

  polkit_backend_authority_revoke_temporary_authorizations (server->authority,
                                                            caller,
                                                            subject,
                                                            &error);
  if (error != NULL)
    {
      egg_dbus_method_invocation_return_gerror (method_invocation, error);
      g_error_free (error);
      goto out;
    }

  _polkit_authority_handle_revoke_temporary_authorizations_finish (method_invocation);

 out:
  if (caller != NULL)
    g_object_unref (caller);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
authority_handle_revoke_temporary_authorization_by_id (_PolkitAuthority        *instance,
                                                       const gchar             *id,
                                                       EggDBusMethodInvocation *method_invocation)
{
  Server *server = SERVER (instance);
  GError *error;
  PolkitSubject *caller;

  error = NULL;

  caller = polkit_system_bus_name_new (egg_dbus_method_invocation_get_caller (method_invocation));

  polkit_backend_authority_revoke_temporary_authorization_by_id (server->authority,
                                                                 caller,
                                                                 id,
                                                                 &error);
  if (error != NULL)
    {
      egg_dbus_method_invocation_return_gerror (method_invocation, error);
      g_error_free (error);
      goto out;
    }

  _polkit_authority_handle_revoke_temporary_authorization_by_id_finish (method_invocation);

 out:
  g_object_unref (caller);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
add_lockdown_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  EggDBusMethodInvocation *method_invocation = EGG_DBUS_METHOD_INVOCATION (user_data);
  GError *error;

  error = NULL;
  polkit_backend_authority_add_lockdown_for_action_finish (POLKIT_BACKEND_AUTHORITY (source_object),
                                                           res,
                                                           &error);

  if (error != NULL)
    {
      egg_dbus_method_invocation_return_gerror (method_invocation, error);
      g_error_free (error);
    }
  else
    {
      _polkit_authority_handle_add_lockdown_for_action_finish (method_invocation);
    }
}

static void
authority_handle_add_lockdown_for_action (_PolkitAuthority               *instance,
                                          const gchar                    *action_id,
                                          EggDBusMethodInvocation        *method_invocation)
{
  Server *server = SERVER (instance);
  const gchar *caller_name;
  PolkitSubject *caller;

  caller_name = egg_dbus_method_invocation_get_caller (method_invocation);
  caller = polkit_system_bus_name_new (caller_name);

  polkit_backend_authority_add_lockdown_for_action (server->authority,
                                                    caller,
                                                    action_id,
                                                    add_lockdown_cb,
                                                    method_invocation);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
remove_lockdown_cb (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  EggDBusMethodInvocation *method_invocation = EGG_DBUS_METHOD_INVOCATION (user_data);
  GError *error;

  error = NULL;
  polkit_backend_authority_remove_lockdown_for_action_finish (POLKIT_BACKEND_AUTHORITY (source_object),
                                                              res,
                                                              &error);

  if (error != NULL)
    {
      egg_dbus_method_invocation_return_gerror (method_invocation, error);
      g_error_free (error);
    }
  else
    {
      _polkit_authority_handle_remove_lockdown_for_action_finish (method_invocation);
    }
}

static void
authority_handle_remove_lockdown_for_action (_PolkitAuthority               *instance,
                                             const gchar                    *action_id,
                                             EggDBusMethodInvocation        *method_invocation)
{
  Server *server = SERVER (instance);
  const gchar *caller_name;
  PolkitSubject *caller;

  caller_name = egg_dbus_method_invocation_get_caller (method_invocation);
  caller = polkit_system_bus_name_new (caller_name);

  polkit_backend_authority_remove_lockdown_for_action (server->authority,
                                                       caller,
                                                       action_id,
                                                       remove_lockdown_cb,
                                                       method_invocation);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
authority_iface_init (_PolkitAuthorityIface *authority_iface)
{
  authority_iface->handle_enumerate_actions                    = authority_handle_enumerate_actions;
  authority_iface->handle_check_authorization                  = authority_handle_check_authorization;
  authority_iface->handle_cancel_check_authorization           = authority_handle_cancel_check_authorization;
  authority_iface->handle_register_authentication_agent        = authority_handle_register_authentication_agent;
  authority_iface->handle_unregister_authentication_agent      = authority_handle_unregister_authentication_agent;
  authority_iface->handle_authentication_agent_response        = authority_handle_authentication_agent_response;
  authority_iface->handle_enumerate_temporary_authorizations   = authority_handle_enumerate_temporary_authorizations;
  authority_iface->handle_revoke_temporary_authorizations      = authority_handle_revoke_temporary_authorizations;
  authority_iface->handle_revoke_temporary_authorization_by_id = authority_handle_revoke_temporary_authorization_by_id;
  authority_iface->handle_add_lockdown_for_action              = authority_handle_add_lockdown_for_action;
  authority_iface->handle_remove_lockdown_for_action              = authority_handle_remove_lockdown_for_action;
}

static void
authority_died (gpointer user_data,
                GObject *where_the_object_was)
{
  Server *server = SERVER (user_data);

  g_object_unref (server);
}

/**
 * polkit_backend_register_authority:
 * @authority: A #PolkitBackendAuthority.
 * @well_known_name: Well-known name to claim on the system bus or %NULL to not claim a well-known name.
 * @object_path: Object path of the authority.
 * @error: Return location for error.
 *
 * Registers @authority on the system message bus.
 *
 * Returns: %TRUE if @authority was registered, %FALSE if @error is set.
 **/
gboolean
polkit_backend_register_authority (PolkitBackendAuthority   *authority,
                                   const gchar              *well_known_name,
                                   const gchar              *object_path,
                                   GError                  **error)
{
  Server *server;
  EggDBusRequestNameReply rn_ret;

  server = SERVER (g_object_new (TYPE_SERVER, NULL));

  server->system_bus = egg_dbus_connection_get_for_bus (EGG_DBUS_BUS_TYPE_SYSTEM);

  server->well_known_name = g_strdup (well_known_name);

  if (well_known_name != NULL)
    {
      if (!egg_dbus_bus_request_name_sync (egg_dbus_connection_get_bus (server->system_bus),
                                           EGG_DBUS_CALL_FLAGS_NONE,
                                           well_known_name,
                                           EGG_DBUS_REQUEST_NAME_FLAGS_NONE,
                                           &rn_ret,
                                           NULL,
                                           error))
        {
          goto error;
        }

      if (rn_ret != EGG_DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
        {
          g_set_error (error,
                       POLKIT_ERROR,
                       POLKIT_ERROR_FAILED,
                       "Could not become primary name owner for %s",
                       well_known_name);
          goto error;
        }
    }

  server->authority = authority;

  /* TODO: it's a bit wasteful listening to all name-owner-changed signals... needs to be optimized */
  server->bus_proxy = egg_dbus_connection_get_object_proxy (server->system_bus,
                                                            "org.freedesktop.DBus",
                                                            "/org/freedesktop/DBus");

  server->bus = EGG_DBUS_QUERY_INTERFACE_BUS (server->bus_proxy);

  server->name_owner_changed_id = g_signal_connect (server->bus,
                                                    "name-owner-changed",
                                                    (GCallback) name_owner_changed,
                                                    server);

  server->authority_changed_id = g_signal_connect (server->authority,
                                                   "changed",
                                                   (GCallback) authority_changed,
                                                   server);

  egg_dbus_connection_register_interface (server->system_bus,
                                          object_path,
                                          _POLKIT_TYPE_AUTHORITY,
                                          G_OBJECT (server),
                                          G_TYPE_INVALID);

  /* take a weak ref and kill server when listener dies */
  g_object_weak_ref (G_OBJECT (server->authority), authority_died, server);

  return TRUE;

 error:
  g_object_unref (server);
  return FALSE;
}


/**
 * polkit_backend_authority_get:
 *
 * Loads all #GIOModule<!-- -->s from <filename>$(libdir)/polkit-1/extensions</filename> to determine
 * what implementation of #PolkitBackendAuthority to use. Then instantiates an object of the
 * implementation with the highest priority and unloads all other modules.
 *
 * Returns: A #PolkitBackendAuthority. Free with g_object_unref().
 **/
PolkitBackendAuthority *
polkit_backend_authority_get (void)
{
  static GIOExtensionPoint *ep = NULL;
  static GIOExtensionPoint *ep_action_lookup = NULL;
  static volatile GType local_authority_type = G_TYPE_INVALID;
  GList *modules;
  GList *authority_implementations;
  GType authority_type;
  PolkitBackendAuthority *authority;
  gchar *s;

  /* define extension points */
  if (ep == NULL)
    {
      ep = g_io_extension_point_register (POLKIT_BACKEND_AUTHORITY_EXTENSION_POINT_NAME);
      g_io_extension_point_set_required_type (ep, POLKIT_BACKEND_TYPE_AUTHORITY);
    }
  if (ep_action_lookup == NULL)
    {
      ep_action_lookup = g_io_extension_point_register (POLKIT_BACKEND_ACTION_LOOKUP_EXTENSION_POINT_NAME);
      g_io_extension_point_set_required_type (ep_action_lookup, POLKIT_BACKEND_TYPE_ACTION_LOOKUP);
    }

  /* make sure local types are registered */
  if (local_authority_type == G_TYPE_INVALID)
    {
      local_authority_type = POLKIT_BACKEND_TYPE_LOCAL_AUTHORITY;
    }

  /* load all modules */
  modules = g_io_modules_load_all_in_directory (PACKAGE_LIB_DIR "/polkit-1/extensions");

  /* find all extensions; we have at least one here since we've registered the local backend */
  authority_implementations = g_io_extension_point_get_extensions (ep);

  /* the returned list is sorted according to priority so just take the highest one */
  authority_type = g_io_extension_get_type ((GIOExtension*) authority_implementations->data);
  authority = POLKIT_BACKEND_AUTHORITY (g_object_new (authority_type, NULL));

  /* unload all modules; the module our instantiated authority is in won't be unloaded because
   * we've instantiated a reference to a type in this module
   */
  g_list_foreach (modules, (GFunc) g_type_module_unuse, NULL);
  g_list_free (modules);

  /* First announce that we've started in the generic log */
  openlog ("polkitd",
           LOG_PID,
           LOG_DAEMON);  /* system daemons without separate facility value */
  syslog (LOG_INFO,
          "started daemon version %s using authority implementation `%s' version `%s'",
          VERSION,
          polkit_backend_authority_get_name (authority),
          polkit_backend_authority_get_version (authority));
  closelog ();

  /* and then log to the secure log */
  s = g_strdup_printf ("polkitd(authority=%s)", polkit_backend_authority_get_name (authority));
  openlog (s,
           0,
           LOG_AUTHPRIV); /* security/authorization messages (private) */
  /* Ugh, can't free the string - gah, thanks openlog(3) */
  /*g_free (s);*/

  return authority;
}

void
polkit_backend_authority_log (PolkitBackendAuthority *authority,
                              const gchar *format,
                              ...)
{
  va_list var_args;

  g_return_if_fail (POLKIT_BACKEND_IS_AUTHORITY (authority));

  va_start (var_args, format);
  vsyslog (LOG_NOTICE, format, var_args);

  va_end (var_args);
}
