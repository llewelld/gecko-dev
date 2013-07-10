/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ArgumentsObject_h
#define vm_ArgumentsObject_h

#include "mozilla/MemoryReporting.h"

#include "jsfun.h"

namespace js {

class AbstractFramePtr;

namespace ion {
class IonJSFrameLayout;
}

/*
 * ArgumentsData stores the initial indexed arguments provided to the
 * corresponding and that function itself.  It is used to store arguments[i]
 * and arguments.callee -- up until the corresponding property is modified,
 * when the relevant value is flagged to memorialize the modification.
 */
struct ArgumentsData
{
    /*
     * numArgs = Max(numFormalArgs, numActualArgs)
     * The array 'args' has numArgs elements.
     */
    unsigned    numArgs;

    /*
     * arguments.callee, or MagicValue(JS_OVERWRITTEN_CALLEE) if
     * arguments.callee has been modified.
     */
    HeapValue   callee;

    /* The script for the function containing this arguments object. */
    JSScript    *script;

    /*
     * Pointer to an array of bits indicating, for every argument in 'slots',
     * whether the element has been deleted. See isElementDeleted comment.
     */
    size_t      *deletedBits;

    /*
     * This array holds either the current argument value or the magic value
     * JS_FORWARD_TO_CALL_OBJECT. The latter means that the function has both a
     * CallObject and an ArgumentsObject AND the particular formal variable is
     * aliased by the CallObject. In such cases, the CallObject holds the
     * canonical value so any element access to the arguments object should
     * load the value out of the CallObject (which is pointed to by
     * MAYBE_CALL_SLOT).
     */
    HeapValue   args[1];

    /* For jit use: */
    static ptrdiff_t offsetOfArgs() { return offsetof(ArgumentsData, args); }
};

// Maximum supported value of arguments.length. This bounds the maximum
// number of arguments that can be supplied to Function.prototype.apply.
// This value also bounds the number of elements parsed in an array
// initialiser.
static const unsigned ARGS_LENGTH_MAX = 500 * 1000;

/*
 * ArgumentsObject instances represent |arguments| objects created to store
 * function arguments when a function is called.  It's expensive to create such
 * objects if they're never used, so they're only created when they are
 * potentially used.
 *
 * Arguments objects are complicated because, for non-strict mode code, they
 * must alias any named arguments which were provided to the function.  Gnarly
 * example:
 *
 *   function f(a, b, c, d)
 *   {
 *     arguments[0] = "seta";
 *     assertEq(a, "seta");
 *     b = "setb";
 *     assertEq(arguments[1], "setb");
 *     c = "setc";
 *     assertEq(arguments[2], undefined);
 *     arguments[3] = "setd";
 *     assertEq(d, undefined);
 *   }
 *   f("arga", "argb");
 *
 * ES5's strict mode behaves more sanely, and named arguments don't alias
 * elements of an arguments object.
 *
 * ArgumentsObject instances use the following reserved slots:
 *
 *   INITIAL_LENGTH_SLOT
 *     Stores the initial value of arguments.length, plus a bit indicating
 *     whether arguments.length has been modified.  Use initialLength() and
 *     hasOverriddenLength() to access these values.  If arguments.length has
 *     been modified, then the current value of arguments.length is stored in
 *     another slot associated with a new property.
 *   DATA_SLOT
 *     Stores an ArgumentsData*, described above.
 */
class ArgumentsObject : public JSObject
{
  protected:
    static const uint32_t INITIAL_LENGTH_SLOT = 0;
    static const uint32_t DATA_SLOT = 1;
    static const uint32_t MAYBE_CALL_SLOT = 2;

  public:
    static const uint32_t LENGTH_OVERRIDDEN_BIT = 0x1;
    static const uint32_t PACKED_BITS_COUNT = 1;

  protected:
    template <typename CopyArgs>
    static ArgumentsObject *create(JSContext *cx, HandleScript script, HandleFunction callee,
                                   unsigned numActuals, CopyArgs &copy);

    ArgumentsData *data() const {
        return reinterpret_cast<ArgumentsData *>(getFixedSlot(DATA_SLOT).toPrivate());
    }

  public:
    static const uint32_t RESERVED_SLOTS = 3;
    static const gc::AllocKind FINALIZE_KIND = gc::FINALIZE_OBJECT4_BACKGROUND;

    /* Create an arguments object for a frame that is expecting them. */
    static ArgumentsObject *createExpected(JSContext *cx, AbstractFramePtr frame);

    /*
     * Purposefully disconnect the returned arguments object from the frame
     * by always creating a new copy that does not alias formal parameters.
     * This allows function-local analysis to determine that formals are
     * not aliased and generally simplifies arguments objects.
     */
    static ArgumentsObject *createUnexpected(JSContext *cx, ScriptFrameIter &iter);
    static ArgumentsObject *createUnexpected(JSContext *cx, AbstractFramePtr frame);
#if defined(JS_ION)
    static ArgumentsObject *createForIon(JSContext *cx, ion::IonJSFrameLayout *frame,
                                         HandleObject scopeChain);
#endif

    /*
     * Return the initial length of the arguments.  This may differ from the
     * current value of arguments.length!
     */
    uint32_t initialLength() const {
        uint32_t argc = uint32_t(getFixedSlot(INITIAL_LENGTH_SLOT).toInt32()) >> PACKED_BITS_COUNT;
        JS_ASSERT(argc <= ARGS_LENGTH_MAX);
        return argc;
    }

    /* The script for the function containing this arguments object. */
    JSScript *containingScript() const {
        return data()->script;
    }

    /* True iff arguments.length has been assigned or its attributes changed. */
    bool hasOverriddenLength() const {
        const Value &v = getFixedSlot(INITIAL_LENGTH_SLOT);
        return v.toInt32() & LENGTH_OVERRIDDEN_BIT;
    }

    void markLengthOverridden() {
        uint32_t v = getFixedSlot(INITIAL_LENGTH_SLOT).toInt32() | LENGTH_OVERRIDDEN_BIT;
        setFixedSlot(INITIAL_LENGTH_SLOT, Int32Value(v));
    }

    /*
     * Because the arguments object is a real object, its elements may be
     * deleted. This is implemented by setting a 'deleted' flag for the arg
     * which is read by argument object resolve and getter/setter hooks.
     *
     * NB: an element, once deleted, stays deleted. Thus:
     *
     *   function f(x) { delete arguments[0]; arguments[0] = 42; return x }
     *   assertEq(f(1), 1);
     *
     * This works because, once a property is deleted from an arguments object,
     * it gets regular properties with regular getters/setters that don't alias
     * ArgumentsData::slots.
     */
    bool isElementDeleted(uint32_t i) const {
        JS_ASSERT(i < data()->numArgs);
        if (i >= initialLength())
            return false;
        return IsBitArrayElementSet(data()->deletedBits, initialLength(), i);
    }

    bool isAnyElementDeleted() const {
        return IsAnyBitArrayElementSet(data()->deletedBits, initialLength());
    }

    void markElementDeleted(uint32_t i) {
        SetBitArrayElement(data()->deletedBits, initialLength(), i);
    }

    /*
     * An ArgumentsObject serves two roles:
     *  - a real object, accessed through regular object operations, e.g..,
     *    JSObject::getElement corresponding to 'arguments[i]';
     *  - a VM-internal data structure, storing the value of arguments (formal
     *    and actual) that are accessed directly by the VM when a reading the
     *    value of a formal parameter.
     * There are two ways to access the ArgumentsData::args corresponding to
     * these two use cases:
     *  - object access should use elements(i) which will take care of
     *    forwarding when the value is JS_FORWARD_TO_CALL_OBJECT;
     *  - VM argument access should use arg(i) which will assert that the
     *    value is not JS_FORWARD_TO_CALL_OBJECT (since, if such forwarding was
     *    needed, the frontend should have emitted JSOP_GETALIASEDVAR.
     */
    const Value &element(uint32_t i) const;

    inline void setElement(JSContext *cx, uint32_t i, const Value &v);

    const Value &arg(unsigned i) const {
        JS_ASSERT(i < data()->numArgs);
        const Value &v = data()->args[i];
        JS_ASSERT(!v.isMagic(JS_FORWARD_TO_CALL_OBJECT));
        return v;
    }

    void setArg(unsigned i, const Value &v) {
        JS_ASSERT(i < data()->numArgs);
        HeapValue &lhs = data()->args[i];
        JS_ASSERT(!lhs.isMagic(JS_FORWARD_TO_CALL_OBJECT));
        lhs = v;
    }

    /*
     * Attempt to speedily and efficiently access the i-th element of this
     * arguments object.  Return true if the element was speedily returned.
     * Return false if the element must be looked up more slowly using
     * getProperty or some similar method. The second overload copies the
     * elements [start, start + count) into the locations starting at 'vp'.
     *
     * NB: Returning false does not indicate error!
     */
    bool maybeGetElement(uint32_t i, MutableHandleValue vp) {
        if (i >= initialLength() || isElementDeleted(i))
            return false;
        vp.set(element(i));
        return true;
    }

    inline bool maybeGetElements(uint32_t start, uint32_t count, js::Value *vp);

    /*
     * Measures things hanging off this ArgumentsObject that are counted by the
     * |miscSize| argument in JSObject::sizeOfExcludingThis().
     */
    size_t sizeOfMisc(mozilla::MallocSizeOf mallocSizeOf) const {
        return mallocSizeOf(data());
    }

    static void finalize(FreeOp *fop, JSObject *obj);
    static void trace(JSTracer *trc, JSObject *obj);

    /* For jit use: */
    static size_t getDataSlotOffset() {
        return getFixedSlotOffset(DATA_SLOT);
    }
    static size_t getInitialLengthSlotOffset() {
        return getFixedSlotOffset(INITIAL_LENGTH_SLOT);
    }

    static void MaybeForwardToCallObject(AbstractFramePtr frame, JSObject *obj, ArgumentsData *data);
#if defined(JS_ION)
    static void MaybeForwardToCallObject(ion::IonJSFrameLayout *frame, HandleObject callObj,
                                         JSObject *obj, ArgumentsData *data);
#endif
};

class NormalArgumentsObject : public ArgumentsObject
{
  public:
    static Class class_;

    /*
     * Stores arguments.callee, or MagicValue(JS_ARGS_HOLE) if the callee has
     * been cleared.
     */
    const js::Value &callee() const {
        return data()->callee;
    }

    /* Clear the location storing arguments.callee's initial value. */
    void clearCallee() {
        data()->callee.set(zone(), MagicValue(JS_OVERWRITTEN_CALLEE));
    }
};

class StrictArgumentsObject : public ArgumentsObject
{
  public:
    static Class class_;
};

} // namespace js

template<>
inline bool
JSObject::is<js::ArgumentsObject>() const
{
    return is<js::NormalArgumentsObject>() || is<js::StrictArgumentsObject>();
}

#endif /* vm_ArgumentsObject_h */
