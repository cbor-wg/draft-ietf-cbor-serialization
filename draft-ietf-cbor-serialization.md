---
v: 3

title: CBOR Serialization and Determinism
abbrev: CBOR Serialization
docname: draft-ietf-cbor-serialization-latest
cat: std
updates: 8949


date:
consensus: true
stream: IETF
ipr: trust200902
area:  "Applications and Real-Time"
workgroup: CBOR
keyword: cbor
venue:
    group: CBOR
    type: Working Group
    mail: cbor@ietf.org
    arch: "https://mailarchive.ietf.org/arch/browse/cbor/"
    github: "cbor-wg/draft-ietf-cbor-serialization"



author:
- ins: L. Lundblade
  name: Laurence Lundblade
  org: Security Theory LLC
  email: lgl@securitytheory.com


contributor:
- name: Rohan Mahy
  email: rohan.ietf@gmail.com
- name: Joe Hildebrand
  email: hildjj@cursive.net
- name: Wolf McNally
  organization: Blockchain Commons
  email: wolf@wolfmcnally.com
- name: Carsten Bormann
  organization: Universität Bremen TZI
  email: cabo@tzi.org
- name: Anders Rundgren
  email: anders.rundgren.net@gmail.com
- name: Vadim Goncharov
  email: vadimnuclight@gmail.com
- name: Ken Takayama
  email: ken.takayama.ietf@gmail.com

normative:
  RFC2119:

  RFC8949: cbor

  RFC8610: cddl

  IEEE754:
    target: https://ieeexplore.ieee.org/document/8766229
    title: IEEE Standard for Floating-Point Arithmetic
    author:
    - org: IEEE
    date: false
    seriesinfo:
      IEEE Std: 754-2019
      DOI: 10.1109/IEEESTD.2019.8766229

  IANA.cddl:

  IANA.cbor-tags:


informative:
   RFC8392: CWT

   RFC9413:

   RFC9052: COSE

   RFC7049:

   Examples-Repo:
     title: draft-ietf-cbor-serialization
     author:
     - org: IETF CBOR WG
     date: false
     target: https://github.com/cbor-wg/draft-ietf-cbor-serialization/tree/main/examples

   CTAP2:
     title: Client To Authenticator Protocol v2
     target: https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html
     author:
     - org: W3C
     date: false

   NaNBoxing:
     title: Crafting Interpreters
     author:
      -
        fullname: Robert Nystrom
     date: July, 2021
     target: https://craftinginterpreters.com/optimization.html#nan-boxing

   I-D.mcnally-deterministic-cbor:

   UML:
     title: OMG Unified Modeling Language (OMG UML) Version 2.5.1
     date: December, 2017
     target: https://www.omg.org/spec/UML/2.5.1/PDF

   LAM73: DOI.10.1145/362375.362389

   UNICODE-NORM:
    title: Unicode Normalization Forms
    author:
      - ins: K. Whistler
        name: Ken Whistler
    date: 2025-07-30
    target: https://www.unicode.org/reports/tr15/
    seriesinfo:
      Unicode Standard Annex: "#15"

--- abstract

RFC 8949 defines CBOR, a standard for serializing data types such as integers, strings, and arrays into encoded bytes.
CBOR serialization is flexible, allowing data types to be encoded in multiple ways to accommodate deployment in constrained environments.
This document normatively defines one particular serialization, called "preferred-plus serialization," that is suitable for the majority of CBOR-based protocols.
Protocol designers and implementers who choose it need not understand or specify serialization details themselves.
This document also normatively defines a deterministic serialization.
These serializations are largely compatible with those widely implemented by the CBOR community.

This document updates RFC 8949 with a new rule that limits how new tag definitions can affect the CBOR data model.

This document provides clarifications to RFC 8949 regarding big numbers and floating-point NaN handling, along with general background information on serialization, determinism, and CBOR byte-string wrapping.


--- middle

# Introduction {#Introduction}

## Information Model, Data Model and Serialization {#models}

To understand CBOR serialization and determinism, it's helpful to distinguish between the general concepts of an information model, a data model, and serialization.
These are broad concepts that can be applied to other serialization schemes like JSON and ASN.1.

 |  | Information Model | Data Model | Serialization |
 | Abstraction Level | Top level; conceptual | Realization of information in data structures and data types | Actual bytes encoded for transmission |
 | Example | The temperature of something | A floating-point number representing the temperature | Encoded CBOR of a floating-point number |
 | Standards | e.g., {{UML}} | CDDL {{-cddl}} | CBOR {{-cbor}} |
 | Implementation Representation | n/a | API input to CBOR encoder library, output from CBOR decoder library | Encoded CBOR in memory or for transmission |
 {: #tab-models title="Information Model, Data Model and Serialization"}

CBOR doesn't provide facilities for information models.
They are mentioned here for completeness and context.

CBOR defines a palette of basic types, including the usual integers, floating-point numbers, strings, arrays, and maps.
Extended types may be constructed from these basic types.
These basic and extended types are used to construct the data model of a CBOR protocol.
While not required, CDDL may be used to describe the data model of a protocol.

The types in the data model are serialized per {{-cbor}} to create encoded CBOR.


## Flexible Serialization

CBOR intentionally allows multiple valid serializations of the same data item.
For example, the array \[1, 2\] can be serialized in more than one way:

| Type              | Description                                      | Encoded Bytes        |
|-------------------|--------------------------------------------------|----------------------|
| Definite-length   | The array length (2) is encoded at the beginning | 0x820102             |
| Indefinite-length | The array is terminated by the break byte (0xff) | 0x9f0102ff           |
{: #tab-array-ser title="[1, 2] definite-length and indefinite-length serializations"}

Similar flexibility exists for most other CBOR data types.

This flexibility is deliberate: CBOR is designed to allow encodings to be selected according to the constraints and requirements of a particular environment.
For example, indefinite-length serialization is suited for streaming large arrays in constrained environments, where the total length is not known in advance.
Conversely, definite-length serialization makes it easier to decode small arrays in constrained environments.
(CBOR is not unique in this regard; compare ASN.1's BER encoding rules.)

Crucially, CBOR allows &mdash; and even expects &mdash; that some implementations will not support all serialization variants.
JSON also permits variation (1, 1.0, and 0.1e1 represent the same number), but expects every parser to handle all of it, since the variation is there for human readability rather than for ease of implementation in constrained environments.

However, CBOR's flexibility introduces two challenges: interoperability and determinism.


### Interoperability

The interoperability challenge arises because partial implementations are both permitted and expected.
For example, an encoder may produce an indefinite-length array that is sent to a decoder that supports only definite-length arrays.
Both this encoder and decoder are allowed by {{-cbor}}.

Decoders in particular often support only a subset of serialization forms &mdash; whether because they operate in constrained environments,
or because a full general-purpose decoder is substantially more work to implement, especially in languages like C and Rust that lack built-in dynamic arrays, maps, and strings.

In practice, most CBOR usage occurs outside highly constrained environments.
This makes it both feasible and beneficial to define a common serialization suitable for general use.

Protocol specifications can reference this serialization; library implementations can prioritize support for it.

{{PreferredPlusSerialization}} defines that serialization: preferred-plus serialization.


### Determinism

The determinism challenge arises because there are multiple ways to serialize the same data item.
The example serialization of the array \[1,2\] above shows this.
This is a problem in some protocols that hash or sign encoded CBOR.

Many approaches to deterministic serialization are possible, each optimized for different environmental constraints or application requirements.
However, as noted earlier, the majority of CBOR usage occurs outside constrained environments.
It is therefore practical to define a single deterministic serialization suitable for general use.

Protocol specifications can reference this serialization instead of defining their own deterministic encoding rules; library implementations can prioritize support for it.

{{DeterministicSerialization}} defines that serialization: deterministic serialization.


## Unspecified Serialization

Many CBOR-based protocols, such as CWT {{-CWT}}, state no serialization requirements, which leaves open what an implementation needs to support.
Is a CWT decoder required to accept indefinite-length items, for example?

One interpretation of {{-cbor}} is that, absent any specification, a decoder is expected to accept every serialization variant, so that it can decode anything it receives.
{{-cbor}} defines this full set of variants without naming it; this document calls it general serialization ({{GeneralSerialization}}).

In practice, however, CWT decoders often omit support for indefinite lengths (and some other variations) because of the added complexity, and encoders accordingly avoid them.
An encoder emitting indefinite lengths would still be fully conforming, yet could fail against such a decoder.
This has rarely caused problems, but it means interoperability rests on convention rather than on the specifications themselves.
{{Recommendations}} addresses this.


## Relation to RFC 8949

### Serialization

This document defines new serializations rather than updating those in {{-cbor}}.
This approach enables the serialization requirements to be expressed directly in normative {{RFC2119}} language and to be defined entirely in this document.
This approach provides clarity and simplicity for implementers and the CBOR community over the long term.

The serializations defined herein are formally new but largely interchangeable with the way the serializations described in {{-cbor}} are implemented.

For example, preferred serialization ({{Section 4.1 of -cbor}}) is commonly implemented without support for indefinite lengths.
Preferred-plus serialization is effectively the same as preferred serialization without indefinite lengths, so it is largely interchangeable with what is commonly implemented.


### Tags and Data Models

This document updates {{-cbor}} in one way: it limits how new tag definitions can affect data models.
The definitions of tags 2 and 3 (big numbers) in {{Section 3.4.3 of -cbor}} modifies the integer type in the CBOR basic generic data model; this is allowed as a one-time exception.
The new rule preserves data model definitions against later modification by unrelated tag definitions, which might undermine their semantics and upset their previous use.

# Recommendations Summary {#Recommendations}

## Protocol Specifications

### Framework Protocols {#FrameworkProtocols}

Framework protocols are those that offer a set of options and alternatives, with interoperability depending on the sender and receiver making compatible choices.
These protocols sometimes make use of profiles to define interoperability requirements for specific uses.
Framework protocols are sometimes described as toolbox or building-block protocols, reflecting their role as collections of reusable mechanisms rather than end-to-end protocols.
CWT, COSE, EAT, and CBOR itself are examples of framework protocols.

It is RECOMMENDED that CBOR-based framework protocols not state serialization requirements, enabling individual uses and profiles to choose serialization to suit their environments and constraints.

CBOR-based framework protocols MAY impose serialization requirements.
For example, if a protocol is never expected to be deployed in constrained environments where map sorting is too expensive, it may mandate deterministic serialization for all implementations in order to eliminate all serialization variability.

One scenario in particular calls for deterministic serialization in a framework protocol: a design in which the parties independently construct and serialize the data to be hashed or signed, rather than transmitting those exact bytes.
See {{WhenDeterministic}}.
For such a design to function correctly, the framework protocol must require deterministic serialization.
COSE is an example, elaborated upon in {{COSESerialization}}.


### End-to-End Protocols {#EndToEndProtocols}

End-to-end protocols are specified such that interoperability is assured when they are implemented in accordance with their specification.
When such a protocol includes optional features, they are typically selected through real-time negotiation.
Such protocols often have formal interoperability compliance programs or organize multi-vendor interop testing events.
TLS, HTTP, and FIDO are examples of end-to-end protocols.

End-to-end protocols MUST define a serialization strategy that ensures the sender and receiver use interoperable serialization.

The strategy most highly RECOMMENDED is to normatively require preferred-plus serialization &mdash; conformance with {{PreferredPlusEncoding}} and {{PreferredPlusDecoding}}.
If a protocol does not need to be deployed where map sorting is too expensive, requiring deterministic serialization &mdash; conformance with {{DeterministicEncoding}} and {{DeterministicDecoding}} &mdash; is also RECOMMENDED.

An end-to-end protocol MAY instead define its own specialized serialization (see {{SpecialSerializations}}).
In such cases, it MUST explicitly specify the permitted serialization behaviors necessary to ensure interoperability.
For example, if a sender is permitted to use indefinite-length serialization, the protocol MUST require that receivers be capable of decoding indefinite-length items.

As with framework protocols, deterministic serialization may be required for parts of the protocol using hashing or signing.
See {{WhenDeterministic}}.

If no specific serialization is required, general serialization (see {{GeneralSerialization}}) applies by default.
In this case, the sender MAY use any valid serialization, and the receiver MUST be able to decode it.
Defaulting to general serialization is NOT RECOMMENDED, because some serializations like indefinite-lengths are not widely supported.


## Libraries

For the following sections on libraries, implementing preferred-plus serialization means making {{PreferredPlusEncoding}} the default or primary behavior for encoding and fulfilling {{PreferredPlusDecoding}} for decoding.
Similarly, implementing deterministic serialization means making {{DeterministicEncoding}} the default or primary behavior for encoding and fulfilling {{DeterministicDecoding}} for decoding.


### CBOR Libraries

It is RECOMMENDED that CBOR libraries implement preferred-plus serialization.

Preferred-plus serialization is recommended because it is suitable for the majority of CBOR-based protocols.
Also, in practice, preferred-plus serialization is equivalent to preferred serialization {{Section 4.1 of -cbor}} for most use cases.

It is also RECOMMENDED that CBOR libraries offer deterministic serialization either as a selectable option or as the default, as some protocols (for example, COSE) require it.
Relative to preferred-plus serialization, the only additional requirement for deterministic serialization is that encoded maps be sorted.
This recommendation is stronger for environments in which map sorting is easy to implement (for example, Python, Go, and Ruby).

Deterministic serialization requirements are a superset of those for preferred-plus; a CBOR library that implements only deterministic serialization therefore satisfies the recommendation for preferred-plus serialization.

A CBOR library MAY also implement some or all aspects of general serialization (see {{GeneralSerialization}}) thereby enabling support for protocols that use specialized serializations (see {{SpecialSerializations}}).

### Libraries for Framework Protocols

When a framework protocol specification does not mandate a specific serialization, it is RECOMMENDED that a library for it implement preferred-plus serialization.
For example, CWT and COSE do not mandate a serialization, so it is recommended that libraries implementing them use preferred-plus serialization.
Alternatively, a framework protocol library may implement only deterministic serialization if this aligns with its deployment environment and design goals.

When a framework protocol mandates serialization requirements, conforming libraries follow them.
For instance, small parts of COSE require deterministic serialization to function correctly.
See {{COSESerialization}}, which shows how COSE requires deterministic serialization for some parts and, as a framework protocol should, leaves others unconstrained.


### Libraries for End-to-End Protocols

End-to-end protocols are expected to state serialization requirements to ensure interoperability.
Libraries for end-to-end protocols are expected to adhere to them.

If an end-to-end protocol specification does not state serialization requirements, it is RECOMMENDED that the library implement preferred-plus serialization.


# General Serialization {#GeneralSerialization}

This section assigns the name "general serialization" to the complete set of encodings standardized in {{Section 3 of -cbor}}.
The term itself was not explicitly defined in {{-cbor}}, though it does refer to it as "variant-tolerant decoding" in passing.
Any serialization, whether defined in this document or elsewhere, permits only encodings drawn from this set.
General serialization is therefore a superset of them all.
It is described as follows:

* CBOR arguments of any length (for example, the integer 0 may be encoded as 0x00, 0x1800, or 0x190000 and so on).
* Floating-point values encoded at any length (for example, 0.0 can be encoded as half-, single-, or double-precision).
* Both definite- or indefinite-length strings, arrays, and maps.
* Maps with keys in any order.
* Big number representation of values that are also representable using major types 0 and 1 (for example, 0 can be encoded as the big number 0xc24100).

A decoder claiming to support general serialization MUST accept and decode all the various encodings for the data types it supports.


## Default Serialization

{{-cbor}} does not explicitly specify default encoding or decoding requirements.
Nothing in it says what a CBOR library needs to support, or what an implementation of a protocol that gives no serialization requirements, such as CWT, needs to do.

Some readers take {{-cbor}} to imply that a decoder must support general serialization and that an encoder may use any variant.
This is a possible interpretation, but many implementers have not adopted it.
For example, CWT and COSE decoders typically do not support indefinite lengths, and their encoders do not produce them.

It is therefore safer not to treat general serialization as the default, particularly when encoding, since a decoder may not accept every variant.


## When To Use General Serialization {#WhenGeneral}

General serialization is rarely necessary, and support for it is not widespread.
Preferred-plus serialization ({{PreferredPlusSerialization}}) is efficient and supports the full CBOR data model (except non-trivial NaNs; see {{NaNBasics}}), satisfying the vast majority of CBOR use cases.

The main scenario where general serialization is warranted is a protocol that must accommodate highly constrained encoders,
at the cost of requiring decoders &mdash; assumed to be unconstrained &mdash; to support every possible serialization option.

When general serialization is required by a protocol, this SHOULD be stated explicitly.

See also special serializations ({{SpecialSerializations}}), which enable optimization and efficiency for specific use cases without requiring full general serialization support in the decoder.

CBOR libraries may nonetheless wish to support general serialization, as a complete set of serialization forms, to be useful to the broader range of protocols.


# Preferred-Plus Serialization {#PreferredPlusSerialization}

This section defines a serialization named "preferred-plus serialization."

Preferred-plus serialization specifies how each data type is encoded.
Like the rest of CBOR, it does not specify which data types an implementation supports, nor which range of values it supports for a given type.
An implementation is free to support as much or as little of CBOR as its protocol requires; preferred-plus constrains only the encoding of what it does support.

For example, a very small CBOR library might support only the integers 0 to 10 and arrays of length 0 to 10.
Preferred-plus allows this, but requires that those arrays be definite-length and that, over this range, the integer value and the array length each be encoded in the initial byte.

Similarly, preferred-plus places no requirement on the range or precision of floating-point values, but requires that each value it does encode be encoded in exactly one way &mdash; for example, 5.9604644775390625E-8 is always encoded as a half-precision subnormal.

Many protocols use only some data types, and only part of the range of those they use.
Implementers need to ensure that the library they use supports the data types, ranges, and precision their protocol requires, and that its encoding and decoding meet the requirements in this section.
A library intended for general use will typically support most or all data types and values.


## Encoder Requirements {#PreferredPlusEncoding}

1. The shortest form of the CBOR argument (see {{Section 3 of -cbor}}) MUST be used.
   This does not apply when the CBOR item is a floating-point value.
   The following describes how an argument in each range is encoded.
   "ai" is the additional information, the low-order 5 bits of the initial byte.

   * 0..23 is encoded in the same byte as the major type.
   * 24..255 is encoded with one additional byte (ai = 0x18).
   * 256..65535 is encoded with two additional bytes (ai = 0x19).
   * 65536..4294967295 is encoded with four additional bytes (ai = 0x1a).
   * 4294967296..18446744073709551615 is encoded with eight additional bytes (ai = 0x1b).

1. Definite-length encoding MUST be used for text and byte strings.

1. Definite-length encoding MUST be used for maps and arrays.

1. Floating-point:

   * Values MUST be encoded in the shortest of double, single, or half-precision that represents the value exactly.
     For example, 0.0 can always be reduced to half-precision so it MUST be encoded as 0xf90000.
     For another example, 0.1 would lose precision if not encoded as double-precision so it MUST be encoded as 0xfb3fb999999999999a.
     Subnormal numbers MUST be used where they give the shortest exact encoding.
   * Encoders MUST NOT output any NaN other than the half-precision NaN 0xf97e00 (sign bit clear, most significant significand bit set, all remaining significand bits clear).
     When a signaling NaN, a NaN with a non-zero payload, or a NaN with the sign bit set is presented to an application or library for encoding, the encoder MUST either reject it or encode it as 0xf97e00.
     Consequently, the floating-point values that can be encoded are the finite numbers, positive and negative infinity, and one NaN.
   * Aside from the requirement allowing only the half-precision quiet NaN, these are the same floating-point requirements as {{Section 4.1 of -cbor}} and also as {{Section 4.2.1 of -cbor}}.
   * Note that this implies that most preferred-plus implementations have to support encoding as single and half-precision.
     Specifically, if the numbers presented for encoding are double-precision, then conversion to single and half-precision is required.
     If the numbers presented for encoding are only single-precision, then conversion to half-precision is required.

1. Big numbers (tags 2 and 3):

   * Leading zeros MUST NOT be encoded.
   * If a value can be encoded using major type 0 or 1, then it MUST be encoded with major type 0 or 1, never as a big number.


## Decoder Requirements {#PreferredPlusDecoding}

If a decoder claims to support preferred-plus serialization it MUST meet these requirements for the data types and ranges it has chosen to support.
A preferred-plus decoder MAY accept non-preferred-plus input.
See {{CheckingDecoder}}.
A decoder SHOULD support the same types and ranges as its corresponding encoder, if it has one.

1. The shortest-form argument MUST be accepted for all major types.

1. If arrays or maps are accepted, definite-length arrays or maps MUST be accepted.

1. If text or byte strings are accepted, definite-length text or byte strings MUST be accepted.

1. If floating-point numbers are accepted, the following apply:

   * If the range of double-precision is supported, finite double-, single-, and half-precision values MUST be accepted.
   * If the range of single-precision is supported, finite single- and half-precision values MUST be accepted.
   * Half-precision NaN (0xf97e00), Infinity (0xf97c00), and -Infinity (0xf9fc00) MUST be accepted.
     These are the only forms of these values a preferred-plus encoder can produce, so accepting their single- and double-precision forms is allowed, but not required.
   * No requirement is made on how a decoded floating-point value is represented to the layer(s) above the decoder; conversion from double, single, or half-precision into that representation may be necessary.

1. If big numbers (tags 2 and 3) are accepted, the following apply. (This is a maximally permissive acceptance mode, adopted to compensate for ambiguity in {{Section 3.4.3 of -cbor}}.)
   * Big numbers described in {{Section 3.4.3 of -cbor}} MUST be accepted. This includes:
       * Leading zeros MUST be ignored.
   * For full interoperability, an empty byte string MUST be accepted and treated as the value zero.

See  {{BigNumbersDataModel}} and {{BigNumberStrategies}} for further background on big numbers, {{BigNumbersCDDL}} for the CDDL specification, and {{CheckingDecoder}} for overrides to the above decoding rules for serialization-checking decoders.


## When to use preferred-plus serialization

Preferred-plus is the recommended default.
It can serialize all CBOR data types and value ranges (except non-trivial NaNs), encodes compactly, is straightforward to implement, and is widely supported by CBOR libraries.
It provides strong serialization interoperability because (1) decoders are required to accept all encodings that a preferred-plus encoder is permitted to produce, and (2) the requirements are formally specified.

Choose a different serialization only when you have a specific need: deterministic serialization when determinism is required, a special serialization with indefinite lengths when streaming is required,
or another special serialization for capabilities beyond what preferred-plus provides (see {{SpecialSerializations}}).
Note that preferred-plus is equivalent to the deterministic serialization of {{DeterministicSerialization}} when maps are not in use.


## Relation To Preferred Serialization {#RelationToPreferred}

Preferred-plus serialization is defined to be the long-term replacement for preferred serialization ({{Section 4.1 of -cbor}}).

The differences are:

* Definite lengths are a requirement, not a preference.
* The only NaN allowed in encoded output is the half-precision quiet NaN.
* For big numbers, leading zeros must be ignored and the empty string must be accepted as zero.

These differences are not of significance in real-world implementations, so preferred-plus serialization is already largely supported.

{{Section 3 of -cbor}} states that in preferred serialization the use of definite-length encoding is a "preference", not a requirement.
Technically that means preferred serialization decoders must support indefinite lengths, but in reality many do not.
Indefinite lengths, particularly for strings, are often not supported because they are more complex to implement than other parts of CBOR.
Because of this, the implementation of most CBOR protocols use only definite lengths.

Further, much of the CBOR community didn't notice the use of the word "preference" and realize its implications for decoder implementations.
It was somewhat assumed that preferred serialization didn't allow indefinite lengths.
That preferred serialization decoders are technically required to support indefinite lengths wasn't noticed by many implementers until several years after the publication of {{-cbor}}.

Briefly stated, the reason that the divergence on NaNs is not of consequence in the real world, is that their non-trivial forms are used extremely rarely and support for them in programming environments and CBOR libraries is unreliable.
See {{NaNCompatibility}} for a detailed discussion.

Thus preferred-plus serialization is largely interchangeable with preferred serialization in the real world.


# Deterministic Serialization {#DeterministicSerialization}

This section defines a serialization named "deterministic serialization"

Deterministic serialization is the same as described in {{Section 4.2.1 of -cbor}} except for the encoding of floating-point NaNs.
See {{PreferredPlusSerialization}} and {{NaN}} for details on, and the rationale for NaN encoding.

Note that in deterministic serialization, any big number that can be represented as an integer must be encoded as an integer.
This rule is inherited from preferred-plus serialization ({{PreferredPlusSerialization}}), just as {{Section 4.2.1 of -cbor}} inherits this requirement from preferred serialization.

See also {{DeterministicConsiderations}} for considerations involved in designing a deterministic protocol that extend beyond serialization.



## Encoder Requirements {#DeterministicEncoding}

1. All of preferred-plus serialization defined in {{PreferredPlusEncoding}} MUST be used.

1. If a map is encoded, the items in it MUST be sorted in the bytewise lexicographic order of their deterministic encodings of the map keys.
   (Note that this is the same as the sorting in {{Section 4.2.1 of -cbor}} and not the same as {{Section 3.9 of RFC7049}} / {{Section 4.2.3 of -cbor}}.)

## Decoder Requirements {#DeterministicDecoding}

1. Decoders MUST meet the decoder requirements described in {{PreferredPlusDecoding}}.
That is, deterministic encoding imposes no requirements over and above the requirements for decoding preferred-plus serialization.

## When to use Deterministic Serialization {#WhenDeterministic}

### Not Commonly Needed for Hashing and Signing

Most applications do not require deterministic encoding &mdash; even those that employ signing or hashing to authenticate or protect the integrity of data.
For example, the payload of a COSE_Sign message (See {{-COSE}}) does not need to be encoded deterministically because it is transmitted with the message.
The recipient receives the exact same bytes that were signed.

Deterministic encoding becomes necessary only when the protected data is not transmitted as the exact bytes that are used for authenticity or integrity verification.
In such cases, both the sender and the receiver must independently construct the exact same sequence of bytes.
To guarantee this, the encoding must eliminate all variability and ambiguity.
The Sig_structure, defined in {{Section 4.4 of -COSE}}, is an example of this requirement.
Such designs are often chosen to reduce data size, preserve privacy, or meet other design constraints.

See the more detailed, COSE-based example in {{COSESerialization}}.

### Decoding Deterministic Serialization and Relation to Preferred-Plus Serialization

The only difference between preferred-plus and deterministic serialization is that in deterministic serialization, maps are required to be sorted by their keys.
Preferred-plus serialization exists as a separate mode solely because map sorting can be too expensive in some constrained environments.

Map decoding must never depend on the sort order of a map, even when maps are required to be sorted.
As a result, deterministic serialization ({{DeterministicSerialization}}) can always be decoded by a decoder that supports preferred-plus serialization ({{PreferredPlusSerialization}}).
Because of this property, deterministic serialization can always be used in place of preferred-plus serialization.
In environments where map sorting is not costly, it is both acceptable and beneficial to always use deterministic serialization.
In such environments, a CBOR encoder may produce deterministic encoding by default and may even omit support for preferred-plus encoding entirely.

However, note that deterministic serialization is never a substitute for general serialization where use cases may require indefinite lengths, separate big numbers from integers in the data model, or need non-trivial NaNs.


### No Map Ordering Semantics

In the basic generic data model, maps are unordered (See {{Section 5.6 of -cbor}}).
Applications MUST NOT rely on any particular map ordering, even if deterministic serialization was used.
A CBOR library is not required to preserve the order of keys when decoding a map, and the underlying programming language may not preserve map order either &mdash; for example, the Go programming language provides no ordering guarantees for maps.
The sole purpose of map sorting in deterministic serialization is to ensure reproducibility of the encoded byte stream, not to provide any semantic ordering of map entries.
If an application requires a map to be ordered, it is responsible for applying its own sorting.


# Special Serializations {#SpecialSerializations}

When needed, protocols may define special serializations beyond the three described above.
The main capabilities they enable are:


* Streaming encoding of text strings, byte strings, arrays, and maps using indefinite lengths, for use when the encoded item(s) exceeds the memory available on the encoding device.

* Fixed-size integer encoding, allowing values to be copied directly to and from hardware registers.
CBOR is simple enough that encoders and decoders for some protocols can be implemented entirely in hardware.

* Fixed-width floating-point encoding, relieving the encoder from performing floating-point reduction to the shortest representable form.

* In-place length updates for strings, arrays, and maps, by encoding their lengths in a fixed number of bits.
For example, if a string length is always encoded in 32 bits, increasing its length from 2^16-1 to 2^16 requires only overwriting the length field rather than shifting all 2^16 bytes of content.

* Transmission of non-trivial NaN floating-point values (see {{NaN}}).

* Deterministic serialization with any or all of the above.

All of these except determinism are also available with general serialization, but a targeted special serialization will usually be substantially easier to implement.

A recommended approach is to define a special serialization as preferred-plus or deterministic serialization with additional constraints or extensions.
For example, a protocol requiring deterministic streaming of maps and arrays could be specified as:

>> Deterministic serialization MUST be used, except that maps and arrays MUST be encoded with indefinite lengths.
>> Strings retain definite-length encoding, and map ordering MUST follow the rules of deterministic serialization.


# New Tag Data Model Rule {#TagDataModelRule}

[^to-be-removed4]

[^to-be-removed4]: This section is new in draft-03. The author thinks it may be out of place in this document, but there's no other good place for it yet.

{{Section 2 of -cbor}} states that each new CBOR tag definition introduces a new and distinct data type.
In contrast, the definitions of Tags 2 and 3 (bignums) in {{Section 3.4.3 of -cbor}} do not introduce a separate data type; instead, they attach directly to the integer type and extend its numeric range.
As a result, the generic data model’s integer type is modified rather than augmented with a new, independent type (see {{BigNumbersDataModel}}).

This document establishes a new rule that prohibits future tag definitions from having such effects:

All future CBOR tag definitions MUST NOT incorporate, modify, or otherwise affect any data types other than the type defined by the tag itself.
A set of tags MAY affect each other, provided that all defining authorities for those tags explicitly agree.

Tags 2 and 3 are exempt from this rule, as they were defined prior to the establishment of this requirement.

# CDDL Serialization Control Operator {#CDDL-Operators}

The ".serial" control operator specifies the serialization of any type in CDDL, including the serialization of the whole document or byte-string-wrapped sub-parts.

The controller (the right-hand side) MUST be either "prefp" or "dtrm", specifying preferred-plus ({{PreferredPlusSerialization}}) or deterministic ({{DeterministicSerialization}}) serialization, respectively.

The scope of .serial applies recursively through nested arrays and maps, but does not extend into byte strings or other data items that happen to contain encoded CBOR.
Every instance of embedded CBOR that requires specific serialization must specify it explicitly.
See also {{ByteStringWrapping}}.

For example, the following specifies that a message or protocol described by "stuff" is deterministically serialized and wrapped in a byte string:

~~~
stuff = ...
deterministic-stuff = stuff .serial "dtrm"
wrapped-deterministic-stuff = #6.24(bytes .cbor deterministic-stuff)
~~~

For another example, the first lines of a CDDL document as follows specify that "my-protocol" be serialized with preferred-plus.

~~~
my-prefp-protocol = my-protocol .serial "prefp"
my-protocol = ...
~~~

New controller values for new serializations are possible but are highly discouraged.
Standards action is required to add them.


# Security Considerations

The security considerations in {{Section 10 of -cbor}} apply.

## Covert Channel {#CovertChannels}


CBOR’s serialization variants can be used as a covert channel {{LAM73}} to steganographically exfiltrate data.

For example, a CBOR argument (such as an integer encoding or a string length) can be encoded up to five different ways (e.g. the value 1 can be encoded as 0x01, 0x1801, 0x190001, 0x1A00000001, or 0x1B0000000000000001).
This variability can be used to encode ~2 hidden bits per argument.
Since every CBOR item carries an argument, even moderately complex protocols may accumulate sufficient bits to exfiltrate sensitive material (e.g., cryptographic keys) or to generate persistent identifiers for tracking users or devices.

The following techniques may be used to establish a covert channel:

* Varying the encoding of CBOR arguments (as above)
* Representing text or byte strings as indefinite-length encodings, and encoding information in the segmentation structure (e.g., segment lengths or insertion of empty segments)
* Encoding data within NaN payloads
* Manipulating the ordering of map entries
* Varying the Unicode representation of text strings

These channels are covert because most CBOR decoders accept all such representations without raising errors or warnings.¶

The primary safeguard is to ensure the CBOR encoding library used is trustworthy and does not exfiltrate data.

Another option is to require preferred-plus or deterministic serialization and to have decoders issue a warning if not compliant.
See {{CheckingDecoder}}.


# IANA Considerations

[^to-be-removed]

[^to-be-removed]: RFC Editor: please replace RFCXXXX with the RFC
    number of this RFC and remove this note.

This document requests IANA to register the ".serial" control operator into the registry "{{cddl-control-operators (CDDL Control Operators)<IANA.cddl}}" of the {{IANA.cddl}} registry group.

IANA is requested to add a reference to {{TagDataModelRule}} to the CBOR tag registry {{IANA.cbor-tags}}.

--- back


# Specifying a Fully Deterministic Protocol {#DeterministicConsiderations}

While deterministic serialization ({{DeterministicSerialization}}) is sufficient to make most protocols fully deterministic, it is not sufficient for all.
This appendix describes some issues that may need further requirements.
Note that these issues occur for parts of the protocol that the sender and receiver construct independently.
See {{WhenDeterministic}}.

## Protocol Data Definition

For a protocol to be deterministic, its definition at the data model layer must be deterministic.

Here’s an example definition:

>> At the sender’s convenience, the birth date MAY be encoded either as an integer epoch date or string date. The receiver MUST decode both formats.

While this definition is interoperable, it lacks determinism.
The definition leaves a choice open, just as CBOR leaves encoding choices open when deterministic serialization is not required.

To make this example definition deterministic, specify one date format and prohibit the other.

A more interesting source of variability is CBOR's variety of number types.
For instance, the number 2 can be represented as an integer, float, big number, decimal fraction and others defined by tags in the CBOR tag registry.
Most protocol designs will just specify one number type to use, and that will give determinism, but here’s an example specification that doesn’t:

>> At the sender’s convenience, the fluid level measurement MAY be encoded as an integer or a floating-point number. This allows for minimal encoding size while supporting a large range. The receiver MUST be able to accept both integers and floating-point numbers for the measurement.

Again, this ensures interoperability but not determinism &mdash; identical fluid level measurements can be represented in more than one way.
Determinism can be achieved by allowing only floating-point, though that doesn’t minimize encoding size.

A better solution requires the fluid level always be encoded using the smallest representation.
For example, a fluid level of 2 is always encoded as an integer, never as a floating-point number; and a level of 2.000001 is always encoded as a floating-point number so as not to lose precision.
See the numeric reduction defined by {{I-D.mcnally-deterministic-cbor}}.


## Text Strings and Unicode

CBOR's text string type (major type 3) carries UTF-8, which is itself unambiguous.
Unicode is not: the same text may be written as different sequences of code points, since a character with a diacritic can appear either precomposed or as a base character plus combining marks.
"é" is either U+00E9 or U+0065 plus U+0301.
As with the number types above, a deterministic protocol must choose one representation &mdash; typically by requiring a Unicode normalization form like Unicode Normalization Form C (NFC) {{UNICODE-NORM}}.


## CBOR Tags

Some tags allow their content to be represented in more than one way.
For example, the epoch date (tag 1) allows either integer or floating-point representation.
A deterministic protocol using tags should examine each tag's definition for such variability and define a deterministic rule for selecting among the alternatives it finds.


# IEEE 754 NaN {#NaN}

This section provides background information on {{IEEE754}} NaN (Not a Number) and its use in CBOR.


## Basics {#NaNBasics}

{{IEEE754}} defines the most widely used representation for floating-point numbers.
It includes special values for infinity and NaN (Not a Number).
NaN is designed to represent the result of invalid operations, such as the square root of a negative number.

A NaN is not a single value the way positive infinity is.
It has a sign bit and a trailing significand field &mdash; 10 bits in half precision, 23 in single, 52 in double &mdash; whose use is not formally defined.
The design intent is that these bits distinguish NaN types and hold diagnostic detail about a local computation.

IEEE 754 formally defines the notions of quiet and signaling NaN, but the bit pattern distinguishing them is only a recommendation, directed at CPU designers:

- A quiet NaN has the most significant significand bit set.
- A signaling NaN has that bit clear and at least one other significand bit set, to distinguish it from infinity.
- Either can have a non-zero payload, which is the bits other than the most significant significand bit.
- No recommendation is made for the sign bit.

This recommendation is not universally followed (e.g., PA-RISC and pre-R6 MIPS).

An arithmetic operation on a signaling NaN raises the invalid operation exception and does not preserve it, whereas quiet NaNs are usually propagated with the payload intact.
Implementations vary, but this makes quiet NaNs usable for application-defined purposes in a way signaling NaNs are not.

For this discussion, a non-trivial NaN is a signaling NaN, a NaN with a non-zero payload, or a NaN with the sign bit set, as the host represents it.
A trivial NaN is one that passively indicates that the value is not a number, and nothing more.


## Implementation Support for Non-Trivial NaNs

This section discusses the extent of programming language and CPU support for NaN payloads.

Although {{IEEE754}} has existed for decades, support for manipulating non-trivial NaNs has historically been limited and inconsistent.
Some key points:

- Programming languages:

  - The programming languages C, C++, Java, JavaScript, Python and Rust do not provide APIs to set or extract NaN payloads.
  - IEEE 754 is over thirty years old, enough time for support to be added if there was need.

- CPU hardware:

  - CPUs use the distinction between signaling and quiet NaNs to determine whether to raise exceptions.
  - A non-trivial NaN matching the CPU’s signaling NaN pattern may either trigger an exception or be converted into a quiet NaN.
  - Instructions converting between single and double precision sometimes discard or alter NaN payloads.

As a result, applications that rely on non-trivial NaNs generally cannot depend on CPU instructions, floating-point libraries, or programming environments.
Instead, they usually need their own software implementation of IEEE 754 to encode and decode the full bit patterns to reliably process non-trivial NaNs.


## Use and Non-use for Non-Trivial NaNs

Non-trivial NaNs, excluding signaling NaNs, are not produced by standard floating-point operations.
They are typically created at the application level, where software may take advantage of unused bits in the NaN payload.
Such uses are rare and unusual, but they do exist.

One example is the R programming language, which is designed for statistical computing and therefore operates heavily on numeric data.
R uses NaN payloads to distinguish various error or missing-data conditions beyond standard computational exceptions such as division by zero.

Another example is NaNboxing (see {{NaNBoxing}}), a technique used by some language runtimes &mdash; such as certain JavaScript engines &mdash; to efficiently represent multiple data types within a single 64-bit word by storing type tags or pointers in the NaN payload.
(CBOR can represent such payloads, but NaNboxed pointers are generally not meaningful or portable across machines, and therefore are usually unsuitable for network transmission or file storage.)

CBOR’s NaN-payload support can be leveraged if data from these systems must be transmitted over a network or written to persistent storage.

A designer of a new protocol that makes extensive use of floating-point values might be tempted to use NaN payloads to encode out-of-band information such as error conditions.
For example, NaN payloads could be used to distinguish situations such as sensor offline, sensor absent, sensor error, or sensor out of calibration.
While this is technically possible in CBOR, it comes with significant drawbacks:

- Preferred-plus and deterministic serialization cannot be used for this protocol.
- Support for NaN payloads is unreliable across programming environments and CBOR libraries.
- Values cannot be translated directly to JSON, which does not support NaNs of any kind.


## Clarification of RFC 8949

This is a clarifying restatement of how NaNs are to be treated according to {{-cbor}}.

NaNs represented in floating-point values of different lengths are considered equivalent in the basic generic data model if:

 - Their sign bits are identical, and
 - Their significands are identical after both significands are zero-extended on the right to 64 bits

This equivalence is established for the entire CBOR basic generic data model.
A NaN encoded as half-, single-, or double-precision is equivalent whenever it satisfies the rules above.
This remains true regardless of how a CBOR library accepts, stores, or presents a NaN in its API.
At the application layer, the equivalence still holds.
The only way to avoid this equivalence is by using a tag specifically designed to carry NaNs without these equivalence rules, since tags extend the data model unless otherwise specified.

The equivalence is similar to how the floating-point value 1.0 is treated as the same value regardless of the precision used to encode it.
Some floating-point values cannot be represented in shorter formats (e.g., 2.0e+50 cannot be encoded in half-precision).
The same is true for some NaNs.

In preferred serialization, this equivalence MUST be used to shorten encoding length.
If a NaN can be represented equivalently in a shorter form (e.g., half-precision rather than single-precision), then the shorter representation MUST be used.

This equivalence also applies when floating-point values are used as map keys.
A map key encoded as half-precision MUST be considered a duplicate of one encoded as double-precision if they meet the equivalence rules above.

However, this equivalence does not apply to map sorting.
Sorting operates on the fully encoded and serialized representation, not on the abstract data model.

It is {{Section 2 of -cbor}} that establishes this equivalence by stating that the number of bytes used to encode a floating-point value is not visible in the data model.
{{Section 4.1 of -cbor}} defines preferred serialization.
It requires shortest-length encoding of NaNs including instructions on how to do it.
{{Section 5.6.1 of -cbor}} describes how NaNs are treated as equivalent when used as map keys.
These three parts of {{-cbor}} are consistent and are the basis of this restatement.

Since {{Section 4.2.1 of -cbor}}, (Core Deterministic Encoding Requirements), explicitly requires preferred serialization, compliant deterministic encodings must use the shortest equivalent representation of NaNs.

Finally, {{Section 4.2.2 of -cbor}} discusses alternative approaches to deterministic encoding.
It suggests, for example, that all NaNs may be encoded as a half-precision quiet NaN.
This section is distinct from the Core Deterministic Encoding Requirements and represents an optional alternative for handling NaNs.


## Divergence from RFC 8949 {#NaNCompatibility}

Non-trivial NaNs are not permitted in either preferred-plus or deterministic serializations.
This is in contrast to preferred serialization and {{Section 4.2.1 of -cbor}}.

Note that the prohibition of non-trivial NaNs is the sole difference between deterministic serialization ({{DeterministicSerialization}}) and {{Section 4.2.1 of -cbor}}.

The divergence is justified by the following:

- Encoding and equivalence of non-trivial NaNs was a little unclear {{-cbor}}.
- IEEE 754 doesn't set requirements for their handling.
- Non-trivial NaNs are not well-supported across CPUs and programming environments.
- Because preferred serialization of non-trivial NaNs is difficult and error-prone to implement, many CBOR implementations don't encode and/or decode non-trivial NaNs, or don't encode or decode them correctly.
- Practical use cases for non-trivial NaNs are extremely rare.
- Reducing non-trivial NaNs to a half-precision quiet NaN is simple and supported by programming environments (e.g., `isnan()` can be used to detect all NaNs).
- Non-trivial NaNs remain supported by general serialization; the divergence is only for preferred-plus and deterministic serialization.
- A new CBOR tag can be defined in the future to explicitly support them.


## Recommendations for Use of Non-Trivial NaNs

While non-trivial NaNs are excluded from preferred-plus and deterministic serialization, they are supported by {{GeneralSerialization}}.

New protocol designs SHOULD avoid non-trivial NaNs.
Support for them is unreliable, and it is straightforward to design CBOR-based protocols that do not depend on them.
In many cases, the use of NaN can be replaced entirely with null.
JSON requires use of null as it does not support NaNs at all.

The primary use case for non-trivial NaNs is existing systems that already use them.
For example, a program that relies on non-trivial NaNs internally may need to serialize its data to run across machines connected by a network.


# Code for Encoding Preferred-Plus Floating-Point Values

Preferred-plus ({{PreferredPlusEncoding}}) and deterministic serialization require that floating-point values fitting in single-precision and half-precision be encoded as such.
This C code implements that conversion.
While conversion between single and double is widely supported, conversion to half-precision is not.
{{Appendix D of -cbor}} provides example code for decoding half-precision values; this appendix provides corresponding code for encoding them.

Two functions are provided: one to convert from double to single, and another from single to half.
Used together, they cover all possible cases.
If the input is a double, pref_plus_double_to_single() must be called first;
if it succeeds, pref_plus_single_to_half() is then called to complete the conversion from double to half.
If the input is already a single, only pref_plus_single_to_half() need be called.

(The two functions have identical structure.
Because the constants are difficult to compute and verify, both are provided.)

Both functions return an integer with the bit pattern for the resulting floating-point value, or a negative value on failure.
-1 indicates the conversion can't be performed because the input is out of range or precision would be lost.
-2 indicates a non-trivial NaN was given for encoding which should either be rejected or output as a half-precision quiet NaN.

~~~ c
{::include prefp-float-encode.c}
~~~
{: #half-encode title="Example C Code for Preferred-Plus Floating-Point Encoding"}


# Big Numbers and the CBOR Data Model {#BigNumbersDataModel}

The primary purpose of this document is to define preferred-plus and deterministic serialization.
Accordingly, {{PreferredPlusSerialization}} describes CBOR’s unified integer space in terms of serialization behavior.
This is an effective and clear way to describe what implementors must do.
An implementation that follows the requirements in {{PreferredPlusSerialization}} will be complete and correct with respect to serialization.

From a conceptual perspective, however, additional discussion is warranted regarding the CBOR data model itself.
That discussion is provided in this appendix.
(Please review {{models}} for background on the difference between serialization and the data model).

In the basic, generic CBOR data model, each tag represents a distinct data type ({{Section 2 of -cbor}}).
Tags are also distinct from the major types, such as numbers and strings.
By this, an integer value such as 0 or 1 encoded as major type 0 is clearly distinct in the data model from the same integer value encoded as tag 2.

However, the text in {{Section 3.4.3 of -cbor}} overrides this by defining these encodings to be equivalent rather than distinct.
This text therefore modifies the CBOR data model.
No other serialization requirement in {{-cbor}} or in this document alters the data model; this equivalence is the sole exception.
This is unusual because the data model is otherwise orthogonal to serialization.

Further, {{Section 3.4.3 of -cbor}}  along with text in {{Section 2 of -cbor}} are interpreted such that there is never a CBOR data model where there is a distinction between these integer representations.
That is, the equivalence applies regardless of the serialization even though much of the relevant text appears in proximity to discussions of serialization.

This document does not attempt to update or revise the text of {{Section 3.4.3 of -cbor}}.
Rather, it records the commonly accepted interpretation of that text and its implications for the CBOR data model.

This document does create a new rule for future tag definitions.
See {{TagDataModelRule}}.


# CDDL for Big Numbers {#BigNumbersCDDL}

The types bigint and biguint in the CDDL Standard Prelude ({{Appendix D of -cddl}}) do NOT describe the big numbers described in this document or in {{Section 3.4.3 of -cbor}}, not even for general serialization.
The types integer and unsigned can be used, but note that they do not fully express the rules that govern the choice between major types 0 and 1 and the big number tags.
CDDL-described protocols SHOULD use integer and unsigned and in prose state that these values correspond to either {{PreferredPlusSerialization}} of this document or {{Section 3.4.3 of -cbor}}.
{{BigNumbersDataModel}} explains the reasons for this.

The following CDDL can be used:

~~~~
{::include bn.cddl}
~~~~


# Big Number Implementation Strategies {#BigNumberStrategies}

{{BigNumbersDataModel}} describes how CBOR defines a single integer number space, in which big numbers are not distinct from values encoded using major types 0 and 1.
This appendix discusses approaches for implementers to support that model.

Some programming environments provide strong native support for big numbers (e.g., JavaScript, Python, Ruby, and Go), while others do not (e.g., C, C++, and Rust).
Even in environments that support big numbers, operations on native-sized integers (e.g., 64-bit integers) are typically much more efficient.
It is therefore reasonable for a CBOR library to expose separate APIs for native-sized integers and for big numbers.

When a CBOR library provides a big number API, values that fall within the range of major types 0 and 1 must be encoded using those major types rather than tags 2 or 3.
Similarly, decoding facilities that return big numbers must accept values encoded using major types 0 and 1, even though the returned representation is a big number.

Alternatively, some CBOR libraries may choose to return tags 2 and 3 as raw byte strings, as this approach is simpler than implementing full big number support.
When a library adopts this approach, it should clearly document that the application layer is responsible for performing the integer unification.
The application is also responsible for handling CBOR’s offset-by-one encoding of negative values and the extended negative integer range permitted by major type 1.

In most cases, these additional processing steps are straightforward when the application already uses a big number library.

Another acceptable approach is for a CBOR library to provide a generic mechanism that allows applications to register handlers for specific tags.
In this case, handlers for tags 2 and 3 MUST perform the required unification with major types 0 and 1.

Finally, note that big numbers are not a widely used feature of CBOR.
Some CBOR libraries may entirely omit support for tags 2 and 3.


# Serialization Checking {#CheckingDecoder}

Serialization checking rejects input that, while well-formed CBOR, does not conform to a serialization rule set it is enforcing.
For example, a decoder checking for deterministic serialization will error out if map keys are not in the required sorted order.
Likewise, a decoder checking for preferred-plus serialization will reject, for instance, any CBOR data item that is not encoded in its shortest form.

To align with long-settled security practice and defend against malformed input attacks, every CBOR decoder must reject all input that is not well-formed.
Serialization checking goes beyond that.
The data rejected by serialization checking is well-formed; it is rejected only because of additional serialization constraints.


## Serialization Checking Use Cases

Some protocol environments may use serialization checking to minimize representational variants as a strategy to improve interoperability.
Discouraging variants early prevents them from compounding.
See {{RFC9413}} on maintaining robust protocols.

Serialization checking helps defend against covert channels described in {{CovertChannels}}.

Applications that rely on deterministic serialization may use serialization checking to ensure that the data they consume is truly deterministic and that the assumptions their logic makes about determinism hold.

A protocol that depends on deterministic serialization may recommend or require its decoders to perform serialization checking.
CBOR libraries may offer serialization checking as a selectable option, at some cost in code size and processing.

Serialization checking may enhance security in certain contexts, but such checking is never a substitute for complete well-formedness checking.
All CBOR decoders &mdash; regardless of their capabilities, modes, or optional features &mdash; must perform full well-formedness checking.
They must also reject well-formed input that uses features they do not support.
For example, a decoder that does not support indefinite-length items rejects them because they are unsupported, not because it is acting as a checking decoder.

A decoder that fails to perform well-formedness checking is unsafe, whatever else it does.
The appropriate remedy is to fix it, not to add the serialization checking described here.


## Big Number Leading Zero Exception

{{Section 3.4.3 of -cbor}} requires that decoders supporting tags 2 and 3 be able to decode bignums that have leading zeros, even though preferred serialization never produces them.
This conflicts with the goal of a decoder that checks for preferred or deterministic serialization: such a decoder needs to reject a bignum with leading zeros as non-conformant.

This document recommends that decoders performing serialization checking reject bignums containing leading zeros, notwithstanding the MUST in {{Section 3.4.3 of -cbor}}.
Serialization checking is optional.
When a protocol selects it &mdash; for example, because it depends on deterministic encoding &mdash; decoders are expected to perform the check, including rejecting non-preferred bignum encodings such as those with leading zeros.

Note that serialization-checking decoders always reject the empty byte string, because it represents the value zero, which is encoded as major type 0.


# CBOR Byte String Wrapping {#ByteStringWrapping}

This appendix provides non-normative guidance on byte-string wrapping of CBOR.
It applies primarily to tag 24 and the CDDL .cbor and .cborseq control operators, but also to the serialization-specifying control operators described in {{CDDL-Operators}}.
It also applies when prose states the byte-string wrapping requirement, such as for the COSE protected headers.
See also {{COSEPayload}}.

## Purpose

Error isolation:

: Wrapping CBOR in a byte string prevents encoding errors in the wrapped data from causing the enclosing CBOR to fail during decoding.
(CBOR decoding generally halts at the first error and lacks internal length redundancy found in formats like ASN.1/DER.)

CBOR library support for signing and hashing:

: When wrapped CBOR needs to be signed or hashed, its original encoded bytes must be available.
Most CBOR libraries cannot directly extract the raw bytes of substructures, but byte-string wrapping provides direct access to the exact bytes for signing or hashing.

Protocol embedding:

: Byte-string wrapping is generally useful when messages from one CBOR-based protocol need to be embedded within another CBOR protocol.

Special map keys:

: Some CBOR libraries only support simple, non-aggregate map keys (e.g., integers or strings).
To use complex data types like arrays and maps as map keys, they can be wrapped in a byte string.

## Wrapping Recommendations

The serialization requirements for the wrapping CBOR may differ from those for the wrapped CBOR.
CBOR itself imposes no universal rule that they must match; this is determined by the design of the wrapping protocol.

The wrapping protocol should not impose serialization requirements on the wrapped message.
The two should be treated as independent entities.
This approach avoids potential conflicts between serialization rules.

For example, assume protocol XYZ wraps protocol ABC.
If protocol ABC requires Canonical CBOR as specified in {{Section 3.9 of RFC7049}} (e.g., {{CTAP2}} from WebAuthn) while protocol XYZ requires deterministic serialization, {{DeterministicSerialization}}, a conflict would arise.

Most CBOR data to be signed or hashed does not require a specific serialization.
CBOR, being a modern, fully specified, binary protocol, does not need canonicalization, wrapping, or armoring like other data representation formats such as JSON.
See the discussion in {{WhenDeterministic}}.

## CBOR Library Implementation Suggestion

A straightforward implementation strategy is to instantiate a second CBOR encoder or decoder for the wrapped message.
However, this may be suboptimal in memory-constrained environments, as it may require both a duplicate copy of the wrapped data and an additional encoder/decoder instance.

A more efficient approach can be for the CBOR library to treat the wrapped CBOR like a container (similar to arrays or maps).
Many CBOR implementations already handle arrays and maps as containers without requiring a separate instance.
Similarly, a byte-string wrapping encoded CBOR can be treated as a container that always contains exactly one item.


# Signing and Hashing Encoded CBOR

CBOR protocols are generally described as a collection of data items, often using CDDL.
Signing or hashing is performed over some range of the encoded CBOR, possibly all of it.
For signing or hashing to work, the sender/encoder and the receiver/verifier must operate on exactly the same range of encoded bytes, so a protocol definition MUST define that range unambiguously.
There are several ways to specify it:

- Name an existing data item (e.g., a CDDL type)
- Name a data item created specifically for this purpose (e.g., a CDDL type)
- Describe the input data items in prose
- Wrap the input in a byte string

Typically, a named data item is an array or a map, but non-aggregate items can be named as well.
When naming an item, the specification should state explicitly whether the input is the full encoded item (head, contents, and trailing "break" if indefinite-length) or only its contents.
Covering the full encoded item is recommended, as it is clearer.
If the item might be tag content (i.e., preceded by one or more tag numbers), the specification should state whether the tag numbers are included in the input; including them is recommended.

It is also possible to specify a slice of a map or array as the input.
A good way to do this is to define a CDDL type that represents the slice as a CBOR sequence.

Another practice is to wrap the bytes to be signed or hashed in a byte string, following the pattern of tag 24 (the tag number itself is typically unnecessary and omitted).
This makes the input to the signature or hash &mdash; the contents of the byte string &mdash; entirely unambiguous: there are no concerns about definite versus indefinite lengths, serialization variants, or tagging; any or all of these work.

The choice of input bytes also affects implementations and their use of CBOR libraries.

Byte-string wrapping is guaranteed to work with even simple, basic libraries, although it will probably require two instances of the encoder or decoder: one for the wrapped (signed/hashed) CBOR and one for the wrapping CBOR. See {{ByteStringWrapping}}.

If byte-string wrapping is not used, the CBOR library must provide an additional feature that gives access to the undecoded CBOR.
For example, it may offer a "tell()" operation that reports the byte offset of the start of the first covered item and of the end of the last covered item (which is more complicated when indefinite lengths are involved), or it may offer an API that directly returns the undecoded bytes of an item.

Another tactic is to require deterministic encoding.
The receiver then reconstructs the signed/hashed bytes from the decoded data items rather than accessing the received encoded CBOR.

In summary, byte-string wrapping is the most reliable approach because it clearly delineates what is signed or hashed and works with every decoder, but the other designs can also be made to work.


# Serialization for COSE {#COSESerialization}

This appendix highlights how the topics in this document apply to CBOR Object Signing and Encryption  (COSE {{-COSE}}).

It focuses on the COSE_Sign1 message ({{Section 4.2 of -COSE}}), which is sufficient for illustrating the relevant considerations.
COSE_Sign1 is a simple structure for signing a payload.
Its serialization can be described in three parts:

- The payload
- The Sig_structure
- The encoded message (the header parameters and the array of four that is the COSE_Sign1)

## COSE Payload Serialization {#COSEPayload}

The signed payload may or may not be CBOR, but assume that it is, perhaps a CWT or EAT.
The payload is transmitted from the signer/sender fully intact all the way to the verifier/receiver.
Because it is transmitted fully intact, CBOR is a binary protocol and intermediaries do not do things like wrap long lines or add base 64 encoding or such, it is not special in any way and COSE imposes no serialization restrictions on it at all.
That is, it can use any serialization it wants.
The serialization is selected by the protocol that defines the payload, not by COSE.

This highlights the principle that determinism is often NOT needed for signing and hashing described in {{WhenDeterministic}}.

It is also worth noting that the payload is a byte string wrapped.
This is not for determinism, armoring or canonicalization.
It is so that the payload can be any data format, including not CBOR.
It is also so CBOR libraries can return the CBOR-encoded payload for processing by the verification algorithms.
Most CBOR libraries decoders do not provide access to any arbitrary chunk of encoded CBOR in the middle of a message.
This is an example of byte string wrapping described in {{ByteStringWrapping}}.

## COSE Sig_structure {#COSESigStructure}

The Sig_structure {{Section 4.4 of -COSE}} is used to aggregate all the items that are input to the signature algorithm &mdash; the payload, protected headers and other.

The Sig_structure is not transmitted from the sender to the receiver; instead, it is constructed independently by both parties.
COSE therefore explicitly requires deterministic encoding so that both the sender and receiver produce identical encoded CBOR representations.
This requirement is specified in {{Section 9 of -COSE}}.

This COSE requirement is effectively equivalent to the deterministic serialization defined in {{DeterministicSerialization}}, since no floating-point NaNs are involved.
It is also effectively equivalent to preferred-plus serialization as defined in {{PreferredPlusSerialization}}, because the Sig_structure contains no maps.

The determinism requirement does not apply to the protected headers incorporated into the Sig_structure.
Deterministic encoding of the headers is unnecessary because they are transmitted in the exact encoded form in which they are included in the Sig_structure.

Furthermore, determinism requirements do not extend into CBOR inside of byte strings.
Once CBOR data is wrapped in a byte string, its internal encoding is treated as opaque and is not subject to surrounding serialization constraints.

This illustrates the general need for deterministic serialization when signed data is reconstructed rather than transmitted in the exact form that was signed.
See {{WhenDeterministic}}.


## The Encoded Message {#COSEMessage}

A COSE_Sign1 message is an array of four elements containing two header parameter chunks, the payload, and the signature.
The two header parameter chunks are maps that hold the various header parameters.
COSE places no serialization requirements on these elements.
The COSE protocol functions correctly regardless of the CBOR serialization used, as long as the decoder can decode what the encoder sends.

In this respect, the serialization of the COSE_Sign1 message is no different from that of any other CBOR-based protocol message.
Indefinite-length items may be used, and non-shortest CBOR arguments are permitted.
The only requirement is that the serialization used by the encoder be decodable by the receiver.

Strictly speaking, COSE is a framework protocol intended for incorporation into an end-to-end protocol, which should explicitly define its serialization requirements.
See {{FrameworkProtocols}} and {{EndToEndProtocols}}.

In practice, some COSE libraries have implicitly implemented only the preferred (or preferred-plus) serialization, and end-to-end protocols have often defaulted to whatever behavior the underlying COSE library provides.
While this generally works &mdash; particularly because the preferred serialization aligns with the recommendations here &mdash; it is more robust for an end-to-end protocol to state its serialization requirements explicitly.


# Examples

This appendix provides examples of the serializations described in this document.
Each example is a single data item.
Collectively, the examples cover the major CBOR data types and some special cases.
{{tab-example}} describes the five fields provided for each example.

| field | description |
|-|-|
| description| Text describing the item |
| edn-representations | Unencoded value(s) for the data item |
| general-serializations | Encoded representation(s) for general serialization |
| preferred-plus-serializations | Encoded representation(s) for preferred-plus serialization  |
| deterministic-serialization | Encoded representation for deterministic serialization |
{: #tab-example title="Example Data Item Fields"}


## Use for Testing

These examples are designed to support testing of CBOR libraries.
They cover only what is defined in this document and therefore do not provide complete or general CBOR test coverage.
All examples are well-formed and valid.


While the CBOR-encoded serializations for each item can be used directly as test input, the EDN representation usually must be incorporated into the test manually.
For example, the EDN string "-5.0e-324" will likely need to be passed as a value of type double to the API of a C-language CBOR library that encodes double-precision numbers.

Not all CBOR libraries support every data type represented in these examples.
This is acceptable: tests for unsupported types may be skipped, or used to verify that an appropriate “unsupported” error is returned.


### Encode Test

To test encoding, invoke the encoder for each example data item.
The encoder input is the item's EDN representations.
If an example provides multiple EDN representations, each of them should be tested.

The encoder should be either configured for, or to default to, one of the three serialization types described in this document.
A test succeeds if the encoder produces any of the encoded representations given in the example for that serialization type.

If an encoder supports multiple serialization types, each type can be tested in turn.


### Decode Test

To test decoding, invoke the decoder for each example data.

The decoder should be either to be configured for, or to default to, one of the three serialization types described in this document.
For the selected serialization type, process every encoded representation defined for the target type.
A test passes if the decoded output matches the value specified by the corresponding EDN representation.

If a decoder supports multiple serialization types, each type can be tested in turn.


### Checking Decoder Test

Checking decoders are described in {{CheckingDecoder}}.

This test verifies that a checking decoder rejects encodings allowed by general serialization but non-conforming for the target serialization type.
It applies only to CBOR libraries that implement serialization conformance checking.

Testing a checking decoder for a target serialization type is typically performed as follows: for each example, supply the decoder with every representation permitted under general serialization except those allowed for the target serialization type.
Decoding each such input must result in a conformance-checking error.

General-serialization decoders are not tested in this way, since they must accept all valid serialization forms.


### Non-Checking Decoder Test {#NonChecking}

A non-checking decoder may accept encodings beyond what is required for the target serialization type.
For example, a preferred-plus decoder will often accept non-shortest-length arguments, even though it is not required to do so, and such encodings are not permitted under preferred-plus.
These examples can be used to test such extended decoding.

Testing proceeds similarly to that for a checking decoder: inputs outside the target serialization type are supplied to the decoder.
The difference is that, for a non-checking decoder, many of these inputs may successfully decode rather than producing a conformance error.
When decoding does fail, the expected error is typically “unsupported.”

Which encoding forms are accepted and which are rejected as unsupported is entirely dependent on the additional capabilities a CBOR library chooses to support and therefore not specified here.

Note the following:

- It is common for preferred-plus and deterministic decoders to accept non-shortest-length arguments.
- If the floating-point data type is supported, all serialization types described in this document require support for decoding half-precision representations and subnormals.


## Example Data Items

These are available as individual files at {{Examples-Repo}}.

All general serialization examples of strings, arrays and maps include indefinite-length encodings so as to provide full test cases.
CBOR libraries that don't support indefinite-length decoding can not claim to support general serialization even if they support most of the rest of general serialization.
For the purpose of classification by this document they are preferred-plus libraries with extra decoding features.
The extra decoding features can be tested as described in {{NonChecking}}.

File: zero.edn

~~~~
{::include examples/zero.edn}
~~~~

File: three.edn

~~~~
{::include examples/three.edn}
~~~~

File: minus_twenty_five.edn

~~~~
{::include examples/minus_twenty_five.edn}
~~~~

File: 65_bit_neg.edn

~~~~
{::include examples/65_bit_neg.edn}
~~~~

File: byte_string.edn

~~~~
{::include examples/byte_string.edn}
~~~~

File: text_string.edn

~~~~
{::include examples/text_string.edn}
~~~~

File: array.edn

~~~~
{::include examples/array.edn}
~~~~

File: map.edn

Note that map order is not significant in the CBOR data model.
Maps are only sorted to provide deterministic encoding.

~~~~
{::include examples/map.edn}
~~~~

File: map_strings.edn

~~~~
{::include examples/map_strings.edn}
~~~~

File: positive_bignum.edn

Note that big numbers are included in the test data because preferred-plus serialization requires their unification with integers.

~~~~
{::include examples/positive_bignum.edn}
~~~~

File: negative_bignum.edn

Note that this is the value closest that can be represented as a big number, not a type 1 integer for preferred-plus serialization.

~~~~
{::include examples/negative_bignum.edn}
~~~~

File: date_epoch_tag.edn

Note that this is provided as a test case for tags.
There are no requirements for dates in this document.
The tag content in this example does vary by serialization type.

~~~~
{::include examples/date_epoch_tag.edn}
~~~~

File: date_string_tag.edn

Note that this is provided as a test case for tags.
There are no requirements for dates in this document.
The tag content in this example does vary by serialization type.

~~~~
{::include examples/date_string_tag.edn}
~~~~

File: true.edn

~~~~
{::include examples/true.edn}
~~~~

File: simple111.edn

Note that the simple value 111 is of not particular significance.
It was selected because it is an unassigned simple value.

~~~~
{::include examples/simple111.edn}
~~~~

File: float_zero.edn

~~~~
{::include examples/float_zero.edn}
~~~~

File: float_double.edn

~~~~
{::include examples/float_double.edn}
~~~~

File: float_double_subnormal.edn

Note that full subnormal support is required for all serializations defined in this document.

~~~~
{::include examples/float_double_subnormal.edn}
~~~~

File: float_single.edn

~~~~
{::include examples/float_single.edn}
~~~~

File: float_single_subnormal.edn

~~~~
{::include examples/float_single_subnormal.edn}
~~~~

File: float_half.edn

~~~~
{::include examples/float_half.edn}
~~~~

File: float_half_subnormal.edn

~~~~
{::include examples/float_half_subnormal.edn}
~~~~

File: float_neg_infinity.edn

~~~~
{::include examples/float_neg_infinity.edn}
~~~~

File: float_quiet_nan.edn

~~~~
{::include examples/float_quiet_nan.edn}
~~~~

File: float_nan_payload.edn

The NaN payload is a special case.
For preferred-plus and deterministic serialization, the decode should fail.
For general serialization a NaN with a payload should be returned, but there is no EDN representation for that.

~~~~
{::include examples/float_nan_payload.edn}
~~~~







