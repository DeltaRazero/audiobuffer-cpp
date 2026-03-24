#pragma once

// *****************************************************************************

#include <cstdint>
#include <cstdlib>
#include <optional>

#ifndef AUDIO_BUFFER_NONTHREAD_SAFE
  #include <mutex>
#endif

#include "./audio_buffer_base.hpp"

// *****************************************************************************

namespace audiobuffer {

// *****************************************************************************

///
/// @brief An audio buffer container which offers fixed time access to individual channels and samples in any order.
///
/// @tparam T The sample type.
/// @tparam ALLOCATOR_T Allocator class, defaults to `std::allocator`.
///
template <typename T, template<typename> class ALLOCATOR_T=std::allocator>
class AudioBuffer : public AudioBufferBase<T>
{
  // TODO: Assert ALLOCATOR implements the correct interface?.

  // :: PROTECTED ATTRIBUTES :: //

  #ifndef AUDIO_BUFFER_NONTHREAD_SAFE
    // Mutex for protecting the buffer against running multiple dynamic memory
    // allocation operations in parallel.
    std::mutex _alloc_mutex;
  #endif

  // :: CONSTRUCTOR :: //

  public:

  ///
  /// @brief Default constructor.
  ///
  /// @note Initializes an empty buffer without allocating any data.
  ///
  AudioBuffer() : AudioBuffer(0, 0)
  {};

  ///
  /// @brief Data constructor.
  ///
  /// @param frame_count The initial amount of frames.
  /// @param channel_count The initial amount of channels.
  ///
  /// @note When either `channel_count` or `frame_count` has a value of 0, no
  ///   allocations will be done.
  ///
  AudioBuffer(frame_count_t frame_count, channel_count_t channel_count)
  {
    this->_data       = nullptr;
    this->_is_managed = true;

    // Don't do any allocations when unmanaged.
    if (!(channel_count && frame_count)) {
      return;
    }

    // Resizing also clears the values in the buffer.
    this->resize(frame_count, channel_count);
  }

  ///
  /// @brief Copy constructor.
  ///
  /// @param other The audio buffer to copy of the same class type.
  ///
  /// @note If you wish to copy a buffer regardless of the class type, pass the
  ///   value as pointer to deduce it as `AudioBufferInterface`.
  ///
  AudioBuffer (const AudioBuffer& other) : AudioBuffer(&other)
  {}

  ///
  /// @brief Copy constructor with type conversion.
  ///
  /// @param other The audio buffer to copy.
  ///
  AudioBuffer (const AudioBufferInterface* other)
  {
    this->_data       = nullptr;
    this->_is_managed = true;

    if (!other) {
      return;
    }
    if (!other->get_data()) {
      return;
    }

    this->resize(
      other->get_frame_count(),
      other->get_channel_count()
    );
    this->copy_from(other);
  }

  ///
  /// @brief Copy assignment.
  ///
  /// @param other The audio buffer to copy of the same class type.
  ///
  /// @note If you wish to copy a buffer regardless of the class type, pass the
  ///   value as pointer to deduce it as `AudioBufferInterface`.
  ///
  AudioBuffer& operator= (const AudioBuffer& other)
  { return this->operator=(&other); }

  ///
  /// @brief Copy assignment with type conversion.
  ///
  /// @param other The audio buffer to copy.
  ///
  AudioBuffer& operator= (const AudioBufferInterface* other)
  {
    if (!other || this == other) {
      return *this;
    }

    if (!other->get_data()) {
      this->~AudioBuffer();
      this->_is_managed = true;
      return *this;
    }

    this->_is_managed = true;
    this->resize(
      other->get_frame_count(),
      other->get_channel_count()
    );
    this->copy_from(other);

    return *this;
  }

  ///
  /// @brief Move constructor.
  ///
  /// @param other The audio buffer to move.
  ///
  AudioBuffer (const AudioBuffer&& other) noexcept
  {
    this->_data       = other._data;
    this->_is_managed = other._is_managed;
  }

  ///
  /// @brief Move assignment.
  ///
  /// @param other The audio buffer to move.
  ///
  AudioBuffer& operator= (const AudioBuffer&& other) noexcept
  {
    if (this->_data) {
      this->~AudioBuffer();
    }

    this->_data = other._data;
    this->_is_managed = other._is_managed;

    return *this;
  }

  ///
  /// @brief Destructor.
  ///
  ~AudioBuffer()
  {
    #ifndef AUDIO_BUFFER_NONTHREAD_SAFE
      std::lock_guard<decltype(this->_alloc_mutex)> guard(this->_alloc_mutex);
    #endif

    if (!this->_data) {
      return;
    }
    // Only the owning instance managing the data is allowed to deallocate the
    // data struct.
    if (!(this->_is_managed && this->_data->resizable)) {
      return;
    }

    if (this->_data->deallocate) {
      this->_data->deallocate(this->_data);
    }

    ALLOCATOR_T<AudioBufferData> abd_alloc;
    using adb_alloc_t = std::allocator_traits<decltype(abd_alloc)>;
    adb_alloc_t::deallocate(abd_alloc, this->_data, sizeof(AudioBufferData));
    this->_data = nullptr;
  }

  private:

  ///
  /// @brief Function to deallocate dynamically allocated data in an audio buffer data struct.
  ///
  /// @param data The audio buffer data struct to deallocate the dynamically
  ///   allocated fields from.
  ///.
  /// @note Use this only if this instance manages the data inside the data
  ///  struct
  ///
  /// @warning While usage of this function is non-thread safe, it's only called
  ///   by `resize()`, which is thread-safe, and when the data struct goes out
  ///   of scope/is deallocated, which also happens only in thread safe contexts.
  ///
  static void deallocate_data(AudioBufferData* data)
  {
    // Data buffer.
    if (data->buffer) {
      ALLOCATOR_T<typename AudioBufferBase<T>::SAMPLE_T> buffer_alloc;
      using buffer_alloc_t = std::allocator_traits<decltype(buffer_alloc)>;

      buffer_alloc_t::deallocate(
        buffer_alloc,
        static_cast<typename AudioBufferBase<T>::SAMPLE_T*>(data->buffer),
        data->frame_count * data->channel_count
      );
      data->buffer      = nullptr;
      data->frame_count = 0;
    }
    // Channel pointer table.
    if (data->channels) {
      ALLOCATOR_T<typename AudioBufferBase<T>::SAMPLE_T*> channels_alloc;
      using channels_alloc_t = std::allocator_traits<decltype(channels_alloc)>;

      channels_alloc_t::deallocate(
        channels_alloc,
        reinterpret_cast<typename AudioBufferBase<T>::SAMPLE_T**>(data->channels),
        data->channel_count
      );
      data->channels = nullptr;
      data->channel_count = 0;
    }

    return;
  }

  // :: FACTORY METHODS :: //

  public:

  ///
  /// @brief Creates an instance referencing an existing audio buffer of the same type.
  ///
  /// @param data The audio buffer to reference.
  ///
  /// @warning Prevent calling the copy constructor by using `std::move(optional_buffer).value()`.
  ///
  static std::optional<AudioBuffer> from_reference(AudioBufferInterface* audio_buffer) noexcept
  { return from_reference(audio_buffer->get_data()); }

  ///
  /// @brief Creates an instance referencing an existing audio buffer  of the same type.
  ///
  /// @param data The audio buffer data struct to reference.
  ///
  /// @warning Prevent calling the copy constructor by using `std::move(optional_buffer).value()`.
  ///
  static std::optional<AudioBuffer> from_reference(AudioBufferData* data) noexcept
  {
    if (!data) {
      return {};
    }
    if (data->format_id != AudioBufferBase<T>::DESCRIPTOR::FORMAT_ID) {
      return {};
    }

    auto buffer  = AudioBuffer<typename AudioBufferBase<T>::SAMPLE_T, ALLOCATOR_T>(0,0);
    buffer._data = data;
    buffer._is_managed = false;
    return std::make_optional(std::move(buffer));
  }

  // :: INTERFACE METHODS :: //

  public:

  bool resize(frame_count_t frame_count, channel_count_t channel_count=0) audiobuffer__noexcept override final
  {
    // If no data set, presumably from constructor.
    if (!this->_data) {
      if (!(frame_count && channel_count)) {
        return true;
      }
      {
        #ifndef AUDIO_BUFFER_NONTHREAD_SAFE
          std::lock_guard<decltype(this->_alloc_mutex)> guard(this->_alloc_mutex);
        #endif

        ALLOCATOR_T<AudioBufferData> abd_alloc;
        using adb_allocator_t = std::allocator_traits<decltype(abd_alloc)>;

        this->_data = adb_allocator_t::allocate(abd_alloc, sizeof(AudioBufferData));
        // adb_allocator_t::construct(
        //   abd_alloc, this->_data,
        //   // Arguments.
        //   AudioBufferBase<T>::DESCRIPTOR::FORMAT_ID,
        //   true
        // );
        *(format_id_t*)&this->_data->format_id = AudioBufferBase<T>::DESCRIPTOR::FORMAT_ID;
        *(bool*)&this->_data->resizable        = true;
        this->_data->channel_count = 0;
        this->_data->frame_count   = 0;
      }
    }

    // If both buffer size and channel count are zero, we deallocate the memory.
    if (!(frame_count && channel_count)) {
      frame_count   = 0;
      channel_count = 0;
    }
    // Else retain variables which are not explicitly set.
    else {
      frame_count = frame_count
        ? frame_count
        : this->_data->frame_count;
      channel_count = channel_count
        ? channel_count
        : this->_data->channel_count;
    }

    // If the sizes are the same, we don't have to do any reallocations.
    if (frame_count == this->_data->frame_count && channel_count == this->_data->channel_count) {
      return true;
    }

    // If we can't resize.
    if (!this->_data->resizable || this->_data->block_resize) {
      #if (audiobuffer__disable_exceptions)
        return false;
      #else
        this->_data->block_resize
          ? throw std::runtime_error("Cannot resize audio buffer: resize-block flag is set.")
          : throw std::runtime_error("Cannot resize audio buffer: data is not resizable.");
      #endif
    }

    {
      #ifndef AUDIO_BUFFER_NONTHREAD_SAFE
        std::lock_guard<decltype(this->_alloc_mutex)> guard(this->_alloc_mutex);
      #endif

      // Existing buffer data and channel table must be deallocated first before
      // the attributes in the data struct is updated.
      if (this->_data->deallocate) {
        this->_data->deallocate(this->_data);
      }

      // Update metadata with new variables.
      this->_data->frame_count   = frame_count;
      this->_data->channel_count = channel_count;
      this->_data->deallocate    = frame_count ? &this->deallocate_data : nullptr;

      // Then do the allocations, checking if the sizes are indeed set.
      if (frame_count && channel_count) {
        {
          ALLOCATOR_T<typename AudioBufferBase<T>::SAMPLE_T> buffer_alloc;
          using buffer_alloc_t = std::allocator_traits<decltype(buffer_alloc)>;

          auto total_size = static_cast<std::size_t>(frame_count) * channel_count;
          this->_data->buffer = buffer_alloc_t::allocate(buffer_alloc, total_size);
        }
        {
          ALLOCATOR_T<typename AudioBufferBase<T>::SAMPLE_T*> channels_alloc;
          using channels_alloc_t = std::allocator_traits<decltype(channels_alloc)>;

          this->_data->channels = reinterpret_cast<void**>(
            channels_alloc_t::allocate(channels_alloc, channel_count)
          );
          this->_build_channel_table();
        }
      }

      // Initialize with DC center values.clear()
      this->clear();
    }

    return true;
  }

};

// *****************************************************************************

} // namespace audiobuffer
