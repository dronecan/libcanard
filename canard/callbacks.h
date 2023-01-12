/*
 * Copyright (c) 2022 Siddharth B Purohit, CubePilot Pty Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <stdint.h>

namespace Canard {

/// @brief Base class for message callbacks.
template <typename c_msg_type>
class Callback {
public:
    virtual void operator()(const CanardRxTransfer& transfer, const c_msg_type& msg) = 0;
};

/// @brief Static callback class.
/// @tparam c_msg_type type of message handled by the callback
template <typename c_msg_type>
class StaticCallback : public Callback<c_msg_type> {
public:
    /// @brief constructor
    /// @param _cb callback function
    StaticCallback(void (*_cb)(const CanardRxTransfer& transfer, const c_msg_type& msg)) : cb(_cb) {}

    void operator()(const CanardRxTransfer& transfer, const c_msg_type& msg) override {
        cb(transfer, msg);
    }
private:
    void (*cb)(const CanardRxTransfer& transfer, const c_msg_type& msg);
};

/// @brief allocate a static callback object using new
/// @tparam c_msg_type 
/// @param cb callback function
/// @return StaticCallback object
template <typename c_msg_type>
StaticCallback<c_msg_type> *allocate_static_callback(void (*cb)(const CanardRxTransfer& transfer, const c_msg_type& msg)) {
    return (new StaticCallback<c_msg_type>(cb));
}

/// @brief Object callback class.
/// @tparam T type of object to call the callback on
/// @tparam c_msg_type type of message handled by the callback
template <typename T, typename c_msg_type>
class ObjCallback : public Callback<c_msg_type> {
public:
    /// @brief Constructor
    /// @param _obj object to call the callback on
    /// @param _cb callback member function
    ObjCallback(T* _obj, void (T::*_cb)(const CanardRxTransfer& transfer, const c_msg_type& msg)) : obj(_obj), cb(_cb) {}

    void operator()(const CanardRxTransfer& transfer, const c_msg_type& msg) override {
        if (obj != nullptr) {
            (obj->*cb)(transfer, msg);
        }
    }
private:
    T *obj;
    void (T::*cb)(const CanardRxTransfer& transfer, const c_msg_type& msg);
};

/// @brief allocate an object callback object using new
/// @tparam T type of object to call the callback on
/// @tparam c_msg_type type of message handled by the callback
/// @param obj object to call the callback on
/// @param cb callback member function
/// @return ObjCallback object
template <typename T, typename c_msg_type>
ObjCallback<T, c_msg_type>* allocate_obj_callback(T* obj, void (T::*cb)(const CanardRxTransfer& transfer, const c_msg_type& msg)) {
    return (new ObjCallback<T, c_msg_type>(obj, cb));
}

/// @brief Argument callback class.
/// @tparam T type of object to pass to the callback
/// @tparam c_msg_type type of message handled by the callback
template <typename T, typename c_msg_type>
class ArgCallback : public Callback<c_msg_type> {
public:
    /// @brief Constructor
    /// @param _arg argument to pass to the callback
    /// @param _cb callback function
    ArgCallback(T* _arg, void (*_cb)(T* arg, const CanardRxTransfer& transfer, const c_msg_type& msg)) : cb(_cb), arg(_arg) {}

    void operator()(const CanardRxTransfer& transfer, const c_msg_type& msg) override {
        cb(arg, transfer, msg);
    }
private:
    void (*cb)(T* arg, const CanardRxTransfer& transfer, const c_msg_type& msg);
    T* arg;
};

/// @brief allocate an argument callback object using new
/// @tparam T type of object to pass to the callback
/// @tparam c_msg_type type of message handled by the callback
/// @param arg argument to pass to the callback
/// @param cb callback function
/// @return ArgCallback object
template <typename T, typename c_msg_type>
ArgCallback<T, c_msg_type>* allocate_arg_callback(T* arg, void (*cb)(T* arg, const CanardRxTransfer& transfer, const c_msg_type& msg)) {
    return (new ArgCallback<T, c_msg_type>(arg, cb));
}

} // namespace Canard
