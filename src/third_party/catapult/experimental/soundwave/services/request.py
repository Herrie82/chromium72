# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import urllib

import httplib2

from py_utils import retry_util  # pylint: disable=import-error

from services import luci_auth


class RequestError(OSError):
  """Exception class for errors while making a request."""
  def __init__(self, request, response, content):
    self.request = request
    self.response = response
    self.content = content
    message = '%s returned HTTP Error %d: %s' % (
        self.request, self.response.status, self.error_message)
    # Note: often the message will contain unicode characters, so we explicitly
    # encode them as utf-8; otherwise by default python will encode as ascii
    # and choke when trying to display the exception message.
    super(RequestError, self).__init__(message.encode('utf-8'))

  def __reduce__(self):
    # Method needed to make the exception pickleable [1], otherwise it causes
    # the mutliprocess pool to hang when raised by a worker [2].
    # [1]: https://stackoverflow.com/a/36342588
    # [2]: https://github.com/uqfoundation/multiprocess/issues/33
    return (type(self), (self.request, self.response, self.content))

  @property
  def json(self):
    try:
      return json.loads(self.content)
    except StandardError:
      return None

  @property
  def error_message(self):
    try:
      # Try to find error message within json content.
      return self.json['error']
    except StandardError:
      # Otherwise fall back to entire content itself.
      return self.content


class ClientError(RequestError):
  """Exception for 4xx HTTP client errors."""
  pass


class ServerError(RequestError):
  """Exception for 5xx HTTP server errors."""
  pass


def BuildRequestError(request, response, content):
  """Build the correct RequestError depending on the response status."""
  if response['status'].startswith('4'):
    error = ClientError
  elif response['status'].startswith('5'):
    error = ServerError
  else:  # Fall back to the base class.
    error = RequestError
  return error(request, response, content)


@retry_util.RetryOnException(ServerError, retries=3)
def Request(url, method='GET', params=None, data=None,
            content_type='urlencoded', use_auth=False, retries=None):
  """Perform an HTTP request of a given resource.

  Args:
    url: A string with the URL to request.
    method: A string with the HTTP method to perform, e.g. 'GET' or 'POST'.
    params: An optional dict or sequence of key, value pairs to be added as
      a query to the url.
    data: An optional dict or sequence of key, value pairs to send as payload
      data in the body of the request.
    content_type: A string specifying how to encode the payload data,
      can be either 'urlencoded' (default) or 'json'.
    use_auth: A boolean indecating whether to send authorized requests, if True
      luci-auth is used to get an access token for the logged in user.
    retries: Number of times to retry the request in case of ServerError. Note,
      the request is _not_ retried if the response is a ClientError.

  Returns:
    A string with the content of the response when it has a successful status.

  Raises:
    A ClientError if the response has a 4xx stauts, or ServerError if the
    response has a 5xx status.
  """
  del retries  # Handled by the decorator.

  if params:
    url = '%s?%s' % (url, urllib.urlencode(params))

  body = None
  headers = {}
  if data is not None:
    if content_type == 'json':
      body = json.dumps(data, sort_keys=True, separators=(',', ':'))
      headers['Content-Type'] = 'application/json'
    elif content_type == 'urlencoded':
      body = urllib.urlencode(data)
      headers['Content-Type'] = 'application/x-www-form-urlencoded'
    else:
      raise NotImplementedError('Invalid content type: %s' % content_type)
  else:
    headers['Content-Length'] = '0'

  if use_auth:
    headers['Authorization'] = 'Bearer %s' % luci_auth.GetAccessToken()

  logging.info('Making API request: %s', url)
  http = httplib2.Http()
  response, content = http.request(
      url, method=method, body=body, headers=headers)
  if response.status != 200:
    raise BuildRequestError(url, response, content)
  return content
