// Copyright 2016 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "rmw/allocators.h"
#include "rmw/error_handling.h"
#include "rmw/serialized_message.h"
#include "rmw/rmw.h"

#include "fastrtps/subscriber/Subscriber.h"
#include "fastrtps/subscriber/SampleInfo.h"
#include "fastrtps/attributes/SubscriberAttributes.h"

#include "fastcdr/Cdr.h"
#include "fastcdr/FastBuffer.h"

#include "rmw_fastrtps_cpp/custom_subscriber_info.hpp"
#include "rmw_fastrtps_cpp/identifier.hpp"
#include "rmw_fastrtps_cpp/macros.hpp"

#include "./ros_message_serialization.hpp"

extern "C"
{
void
_assign_message_info(
  rmw_message_info_t * message_info,
  const eprosima::fastrtps::SampleInfo_t * sinfo)
{
  rmw_gid_t * sender_gid = &message_info->publisher_gid;
  sender_gid->implementation_identifier = eprosima_fastrtps_identifier;
  memset(sender_gid->data, 0, RMW_GID_STORAGE_SIZE);
  memcpy(sender_gid->data, &sinfo->sample_identity.writer_guid(),
    sizeof(eprosima::fastrtps::rtps::GUID_t));
}

rmw_ret_t
_take(
  const rmw_subscription_t * subscription,
  void * ros_message,
  bool * taken,
  rmw_message_info_t * message_info)
{
  *taken = false;

  if (subscription->implementation_identifier != eprosima_fastrtps_identifier) {
    RMW_SET_ERROR_MSG("publisher handle not from this implementation");
    return RMW_RET_ERROR;
  }

  CustomSubscriberInfo * info = static_cast<CustomSubscriberInfo *>(subscription->data);
  auto error_msg_allocator = rcutils_get_default_allocator();
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    info, "custom subscriber info is null", return RMW_RET_ERROR, error_msg_allocator);

  eprosima::fastcdr::FastBuffer buffer;
  eprosima::fastrtps::SampleInfo_t sinfo;

  if (info->subscriber_->takeNextData(&buffer, &sinfo)) {
    info->listener_->data_taken();

    if (eprosima::fastrtps::rtps::ALIVE == sinfo.sampleKind) {
      eprosima::fastcdr::Cdr deser(
        buffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN, eprosima::fastcdr::Cdr::DDS_CDR);
      _deserialize_ros_message(deser, ros_message, info->type_support_,
        info->typesupport_identifier_);
      if (message_info) {
        _assign_message_info(message_info, &sinfo);
      }
      *taken = true;
    }
  }

  return RMW_RET_OK;
}

rmw_ret_t
rmw_take(const rmw_subscription_t * subscription, void * ros_message, bool * taken)
{
  auto error_msg_allocator = rcutils_get_default_allocator();
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    subscription, "subscription pointer is null", return RMW_RET_ERROR, error_msg_allocator);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    ros_message, "ros_message pointer is null", return RMW_RET_ERROR, error_msg_allocator);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    taken, "boolean flag for taken is null", return RMW_RET_ERROR, error_msg_allocator);

  return _take(subscription, ros_message, taken, nullptr);
}

rmw_ret_t
rmw_take_with_info(
  const rmw_subscription_t * subscription,
  void * ros_message,
  bool * taken,
  rmw_message_info_t * message_info)
{
  auto error_msg_allocator = rcutils_get_default_allocator();
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    subscription, "subscription pointer is null", return RMW_RET_ERROR, error_msg_allocator);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    ros_message, "ros_message pointer is null", return RMW_RET_ERROR, error_msg_allocator);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    taken, "boolean flag for taken is null", return RMW_RET_ERROR, error_msg_allocator);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    message_info, "message info pointer is null", return RMW_RET_ERROR, error_msg_allocator);

  return _take(subscription, ros_message, taken, message_info);
}

rmw_ret_t
_take_serialized_message(
  const rmw_subscription_t * subscription,
  rmw_serialized_message_t * serialized_message,
  bool * taken,
  rmw_message_info_t * message_info)
{
  *taken = false;

  if (subscription->implementation_identifier != eprosima_fastrtps_identifier) {
    RMW_SET_ERROR_MSG("publisher handle not from this implementation");
    return RMW_RET_ERROR;
  }

  CustomSubscriberInfo * info = static_cast<CustomSubscriberInfo *>(subscription->data);
  auto error_msg_allocator = rcutils_get_default_allocator();
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    info, "custom subscriber info is null", return RMW_RET_ERROR, error_msg_allocator);

  eprosima::fastcdr::FastBuffer buffer;
  eprosima::fastrtps::SampleInfo_t sinfo;

  if (info->subscriber_->takeNextData(&buffer, &sinfo)) {
    info->listener_->data_taken();

    if (eprosima::fastrtps::rtps::ALIVE == sinfo.sampleKind) {
      auto buffer_size = static_cast<size_t>(buffer.getBufferSize());
      if (serialized_message->buffer_capacity < buffer_size) {
        auto ret = rmw_serialized_message_resize(serialized_message, buffer_size);
        if (ret != RMW_RET_OK) {
          return ret;  // Error message already set
        }
      }
      serialized_message->buffer_length = buffer_size;
      memcpy(serialized_message->buffer, buffer.getBuffer(), serialized_message->buffer_length);

      if (message_info) {
        _assign_message_info(message_info, &sinfo);
      }
      *taken = true;
    }
  }

  return RMW_RET_OK;
}

rmw_ret_t
rmw_take_serialized_message(
  const rmw_subscription_t * subscription,
  rmw_serialized_message_t * serialized_message,
  bool * taken)
{
  auto error_msg_allocator = rcutils_get_default_allocator();
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    subscription, "subscription pointer is null", return RMW_RET_ERROR, error_msg_allocator);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    serialized_message, "ros_message pointer is null", return RMW_RET_ERROR, error_msg_allocator);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    taken, "boolean flag for taken is null", return RMW_RET_ERROR, error_msg_allocator);

  return _take_serialized_message(subscription, serialized_message, taken, nullptr);
}

rmw_ret_t
rmw_take_serialized_message_with_info(
  const rmw_subscription_t * subscription,
  rmw_serialized_message_t * serialized_message,
  bool * taken,
  rmw_message_info_t * message_info)
{
  auto error_msg_allocator = rcutils_get_default_allocator();
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    subscription, "subscription pointer is null", return RMW_RET_ERROR, error_msg_allocator);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    serialized_message, "ros_message pointer is null", return RMW_RET_ERROR, error_msg_allocator);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    taken, "boolean flag for taken is null", return RMW_RET_ERROR, error_msg_allocator);
  RCUTILS_CHECK_FOR_NULL_WITH_MSG(
    message_info, "message info pointer is null", return RMW_RET_ERROR, error_msg_allocator);

  return _take_serialized_message(subscription, serialized_message, taken, message_info);
}
}  // extern "C"
