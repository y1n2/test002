ARINC

FUNCTION DEFINITON OF AIRBORNE
MANAGER OF AIR-GROUND INTERFACE
COMMUNICATIONS (MAGIC)

ARINC SPECIFICATION 839

PUBLISHED:September 12,2014

Prepared by AEEC
Published by
SAE-ITC
16701 Melford Blvd.,Suite 120,Bowie,Maryland 20715 USA

DISCLAIMER

THIS DOCUMENT IS BASED ON MATERIAL SUBMITTED BY VARIOUS PARTICIPANTS
DURING THE DRAFTING PROCESS.NEITHER AEEC,AMC,FSEMC NOR SAE-ITC HAS
MADE ANY DETERMINATION WHETHER THESE MATERIALS COULD BE SUBJECT TO
VALID CLAIMS OF PATENT,COPYRIGHT OR OTHER PROPRIETARY RIGHTS BY THIRD
PARTIES,AND
MADE IN THIS REGARD.

WARRANTY,EXPRESS

REPRESENTATION

NO

OR

OR

IMPLIED,S

OR

TO

OR

OF

USES

EFFORTS

ACCURACY

TECHNICAL

REASONABLE

ANY
OR
THE

INDUSTRY
THESE

DEVELOP
WARRANTY

ADEQUACY,MERCHANTABLITY,FITNESS

ACTIVITIES
DOCUMENTS.HOWEVER,NO

CERTIFICATION
SUFFICIENCY
FOR
PRODUCTS,COMPONENTS,OR
IN ACCORDANCE

ARINC
MAINTAIN
MADE AS TO THE
THE
SAFETY
RATED,INSTALLED
DOCUMENT
OR
PRODUCTS,COMPONENTS,OR
THIS
ACKNOWLEDGES THAT IT SHALL BE SOLELY RESPONSIBLE FOR ANY LOSS,CLAIM
OR DAMAGE THAT IT MAY INCUR IN CONNECTION WITH ITS USE OF OR RELIANCE
ON
PARTY
AGAINST ANY CLAIM ARISING FROM ITS USE OF THE STANDARD.

AND
IS
OF
DOCUMENTS,
INTENDED
OR
DESIGNED,TESTED,
SYSTEMS
OF THIS
ASPECT
ANY
ASSOCIATEDWITH
SUCH
DOCUMENT
OF

SAE-ITC,AEEC,AMC,FSEMC

WITH
HAZARD
USER

OPERATED
ABSENCE

ANY
HARMLESS

SYSTEMS. THE

DOCUMENT,AND

PARTICIPATED

SHALL
IN

DRAFTING

DOCUMENT

PURPOSE

THIS

HOLD

THAT

RISK

AND

THE

THE

THE

OF

OR

OF

OF

TO

ANY

AFFECT

TERM,SUCH

OF
STATUS

DOCUMENT
THE

THE USE IN THIS
MUST,IS
A
NTENDED
STANDARD OR INANY WAY TO MODIFY THE ABOVE DISCLAIMER.NOTHING HEREIN
SHALL
ANY
REPRESENT THAT THEIR PRODUCTS ARE COMPLIANT WITH THIS STANDARD SHALL
REPRESENTED
BE DEEMED ALSO
CONFORM TO THE FEATURES THAT ARE DESCRIBED AS MUST OR SHALL IN THE
STANDARD.

PRODUCT.HOWEVER,VENDORS

BE DEEMED
OF

AS
DOCUMENT

TO
THIS

EQUIPMENT

PROVIDER

PRODUCTS

STANDARD

REQUIRE

ELEMENT

SHALL

THEIR

THAT

HAVE

THIS

ANY

ITS

TO

OR

OF

AS

IN

CONTAIN

TOINCORPORATE
WHICH

NOT
VOLUNTARY

OR

USE

ANY
SHALL
ACCEPTANCE THEREOF"AS IS"AND BE SUBJECT TO THIS DISCLAIMER.

RELIANCE

DOCUMENT

THIS

OR

ON

OF

CONSTITUTE

AN

documem ispblistod

infomation

askfned by

15

CFR Sction

734.7 ofthe

Export Administation Regulations(EAR). As

ubidy

avaiabe

technology

nde

IS CFR74.3D(3,it

is

nat

Tis
subject

to the EAR and does not have an ECCN. It may be exported without an export

license.

◎2014 BY
SAE INDUSTRY TECHNOLOGIES CONSORTIA (SAE-ITC)
16701 MELFORD BLVD.,SUITE 120,BOWIE,MARYLAND 20715 USA

ARINC

SPECIFICATION

839

FUNCTION DEFINITION OF AIRBORNE MANAGER OF AIR-GROUND

INTERFACE

COMMUNICATIONS(MAGIC)

Published:September

12,2014

Prepared by the Airlines Electronic Engineering Committee(AEEC)
Adopted by the AEEC Executive Commttee

Specification 839

Adoption Date
April 15,2014

Published Date
September 12,2014

FOREWORD

SAE-ITC,the AEEC,and ARINC Standards

ARINC Industry Activities,an industry program of SAE-ITC,organizes aviation industry
committees and participates in related industry activities that benefit aviation at large by
leadership and guidance.These activities directly support aviation
providing technical
industry goals:promote safety,efficiency,regularity,and cost-effectiveness in aircraft
operations.

ARINC Industry Activities organizes and provides the secretariat for international aviation
organizations(AEEC,AMC,FSEMC)which coordinate the work of aviation industry
technical professionals and lead the development of technical standards for airborne
electronic equipment,aircraft maintenance equipment and practices,and flight simulator
equipment used in commercial,military,and business aviation.The AEEC,AMC,and
FSEMC develop consensus-based,voluntary standards that are published by SAE-ITC

and are known as ARINC Standards.The use of ARINC Standards results in substantial
technical and economic benefit to the aviation industry.

There are three classes of ARINC Standards:

a)

ARINC Characteristics-Define the form,fit,function,and interfaces of avionics
and other airline electronic equipment.ARINC Characteristics indicate to
prospective manufacturers of airline electronic equipment the considered and
coordinated opinion of the airline technical community concerning the requisites of
new equipment
foster interchangeability and competition.

including standardized physical and electrical characteristics to

b)ARINC Specifications-Are principally used to define either

the physical

packaging or mounting of avionics equipment,data communication standards,or
a high-level computer language.

c)ARINC Reports-Provide guidelines or general

information found by the airlines

to be good practices,often related to avionics maintenance and support.

The release of an ARINC Standard does not obligate any organization or SAE-ITC to
purchase equipment so described,nor does it establish or indicate recognition or the
existence of an operational requirement for such equipment,nor does it constitute
endorsement of any manufacturer's product designed or built to meet the ARINC
Standard.

In order to facilitate the continuous product

improvement of this ARINC Standard,two

items are included in the back of this volume:

An Errata Report solicits any corrections to existing text or diagrams that may be
included in a future Supplement to this ARINC Standard.

An ARINC IA Project

Initiation/Modification(APIM)form solicits any proposals for

the addition of technical material to this ARINC Standard.

ii

ARINC SPECIFICATION 839
TABLE OF CONTENTS

INTRODUCTION ............................................................................................................................................. 1
Purpose .............................................................................................................................................................. 1
this Standard .......................................................................................... 1
Multi -Phase Development

of

Phase 1 Activities .................................................................................................................................... 1
Future Activities ........................................................................................................................................ 2
Document Organization ............................................................................................................................... 2
....................................................................................................................................3
Reference Document
Document Terminolog ................................................................................................................................. 4

Document Precedence
Regulatory Approval

................................................................................................................................ 4
...................................................................................................................................... 4

MAGIC OVERVIEW AND BENEFITS .................................................................................................. 5
Benefits of MAGIC ......................................................................................................................................... 5
Benefits to Airlines ................................................................................................................................... 5
................................................................................................. 5
Benefits to Airframe Manufacturers
Benefits to Datalink Equipment Suppliers .................................................................................... 5

Benefits to Avionics Application Suppliers ...................................................................................... 5
Concept of MAGIC .........................................................................................................................................5
Overall Functional Scope ............................................................................................................................ 6
Functions for which MAGIC is Responsible .................................................................................. 6
Functions for which MAGIC is Not Responsible ..........................................................................6

Architectural Overview ................................................................................................................................. 6
Centralized MAGIC ................................................................................................................................. 6
Federated MAGIC .....................................................................................................................................7
High -Level Overview .................................................................................................................................... 8
MAGIC Development Phases .................................................................................................................. 10
1......................................................................................................................... 10
........................................................................................... 1
and Data Flow ....................................................................................................................12
Management
Functional Description of MAGIC Operation .................................................................................. 12

Development Phase
Development

of Future Supplements

1.0
1.1
1.2

1.2.1
1.2.2
1.3
1.4
1.5

1.6
1.7

2.0
2.1
2.1.1
2. 1.2
2. 1.3

2. 1.4
2.2
2.3
2.3. 1
2.3.2

2.4
2.4.1
2.4.2
2.5
2.6
2.6.1
2.6.2
2.7
2.8

3.0

3.1
3.2
3.2.1
3.2.2
3.2.3
3.2.4
3.2.4.1

FUNCTIONAL REQUIREMENTS

OF MAGIC .............................................................................. 13

......................................................................................................13
Top -Level Functional Requirement
Top -Level Functional and Interface Description ............................................................................ 13
MAGIC Internal Function .................................................................................................................... 14
MAGIC -Managed External Functions .......................................................................................... 14
MAGIC Interfaces to External Entitie ........................................................................................... 14
High -Level Functional Description .................................................................................................. 15
Functional Description of MAGIC Internal Function ........................................................ 16

3.2.4.1.1
3.2.4.1.1.1
3.2.4.1.1.1.1

3.2.4.1.1.1.2
3.2.4.1.1.2
3.2.4.1.1.3
3.2.4. 1.2
3.2.4. 1.3
3.2.4. 1.4
3.2.4.2

3.2.4.2.1
3.2.4.2.2
3.2.4.2.3

Central Management

............................................................................................................... 16
Client Profiles ........................................................................................................................ 16
Profile for Non -MAGIC -Aware Clients ................................................................ 16

Profile for MAGIC -Aware Clients .......................................................................... 16
Datalink Profiles .................................................................................................................. 17
Central Policy Profile ........................................................................................................ 17
Function .......................................................................................................... 17
Function .................................. 17
...................................................................................................... 17
by MAGIC ........................ 18

Administration
Ground -to -Aircraft Communication Management
Client

Functional Description of External Functions Managed

Interface Controller

Network Queuing , Prioritization ........................................................................................... 18
Firewalling Function ................................................................................................................ 19
Routing Function ........................................................................................................................ 19

iii

3.2.4.2.4
3.2.5
3.2.5. 1
3.2.5.2
3.2.5.3
3.2.5.4
3.2.5.5
3.2.5.6

3.2.5.7
3.2.5.8
3.2.5.9
3.2.5.10
3.3
3.4
3.4.1
3.5
3.6
3.6.1
3.6.1.1
3.6.1.2
3.6.1.3
3.6.1.4

3.6.2
3.6.2.1
3.6.2.2
3.6.2.3
3.6.2.4
3.6.2.5
3.6.2.6

4.0
4.1
4.1.1
4.1.2
4. 1.2. 1
4.1.2.2

4.1.2.2.1
4.1.2.2.2
4.1.2.2.3
4.1.3
4.1.3.1
4.1.3.1.1
4.1.3.1.2
4.1.3.2

4.1.3.2.1
4.1.3.2.2
4.1.3.3
4.1.3.3.1
4.1.3.3.2
4.1.3.4
4.1.3.4.1
4.1.3.4.2

ARINC PROJECT PAPER 839
TABLE OF CONTENTS

Interface Controller
Interface Controller

Data Link Modules .......................................................................................................................19
Interface Description ....................................................................................................... 19
High -Level
to Clients ....................................................................................... 19
Client
Function .................... 20
to Communication Management
Client
to QoS -Engine ................................................. 20
Communication Management
Communication Management Function to Data Link Modules .....................................20
to Network (Routing and Firewalling) ......................................... 21
Central Management
Function Information Sharing ............. 21
Onboard to Ground -Hosted Management

Function

Interface .................................................................................. 21
Aircraft Parameter Retrieval
...................................................................................... 21
Data Link Modules to Link Controller
Federated MAGIC to Federated MAGIC .............................................................................. 21
User Data Interface ......................................................................................................................... 22
..................................................................................................................................22
Policy Considerations
Priority Mode .................................................................................................................................................. 23
ion……………………24
................................................................................... 26
................................................................................................................ 27

Certification and Partitioning Consideration
and Constraints
Assumptions

MAGIC Profile Example Based on AGIE Prioritized

Path -Select..

.................................................................................................. 27
Concept of Data Flow Operation
( DFO 1) ................................................................................. 27
Central MAGIC Data Transfer
Central MAGIC Priority IP Messaging ( DFO 2) ................................................................. 28
(DFO 3) .......................................................................... 28
Federated MAGIC Data Transfer
Federated MAGIC Priority IP Messaging ( DFO 4).............................................................. 29
Concept of Management Flow Operation .................................................................................. 30
Airline Business Parameters Change (MFO 1) ................................................................. 30
(MFO 2) ...................................................................... 31
Develop New MAGIC -Aware Client
Develop New MAGIC -Aware Link ( MFO 3) ............................................................................ 32
( MFO 4).......................................................... 32
Add New Client
Add Communication
Integrate New MAGIC Service with an Existing Environment

( MFO 5) ....................................33
(MFO 6).................... 33

Link to MAGIC Implementation

to MAGIC Implementation

MAGIC INTERFACE DEFINITIONS........................................................................................................ 34
Interface .............................................................................................................................................. 34
Client

34

Authentication
MAGIC Client

and Authorization Mod l
Interface Command Overview .......................................................................... 35
Start and Normal Operation ........................................................................................................ 36
'Event -Driven ' Operati n........................................................................................... 38
Additional
............................................................................................... 38
Client Not Available /Restart
Change of Session Parameters .......................................................................................... 38
Link Controller Failure ............................................................................................................ 38
MAGIC Diameter Command Definition ....................................................................................... 39
Client Authentication Command Pair ....................................................................................... 39
........................................................................................... 39
.............................................................................................. 40
Communication Change Command Pair .............................................................................. 41
...................................................................................... 41
....................................................................................... 42
Notification Message Command Pair ..................................................................................... 42
..................................................................................................................... 42
.................................................................................................................. 43

Communication
Change Request
Communication Change Answer

Client Authentication
Request
Client Authentication Answer

Notification Report
Notification Answer

Status Change Message Command Pair ............................................................................ 43
Status Change Repor .............................................................................................................. 43
Status Change Answer ............................................................................................................ 4

iv

ARINC SPECIFICATION 839
TABLE OFCONTENTS

4. 1.3.5
4.1.3.5.1
4.1.3.5.2
4.1.3.6
4.1.3.6.1
4.1.3.6.2
4.1.3.7
4.1.3.7.1
4.1.3.7.2
4.1.4
4.1.4.1
4.1.4.2
4.2
4.2.1
4.2.2
4.2.2.1
4.3
4.3.1
4.3.1. 1
4.3.1.2
4.3.1.3
4.3.1.4
4.3.2
4.4
4.5
4.6
5.0
5.1
5.1.1
5.2
5.2.1
5.2.2
5.3
5.3.1

Status Message Command Pair ............................................................................. 44
Status Request .................................................................................................... 44
Status Answer ...................................................................................................... 45
Accounting Data Message Command Pair .......................................................... 46
Accounting Data Reques..................................................................................... 46
Accounting Data Answer .................................................................................... 47
Accounting Control Message Command Pair ...................................................... 47
Accounting Control Reques............................................................................... 48
Accounting Control Answer............................................................................... 48
Attribute Value Pairs(AVP.s . )…………………………………………………………48
Diameter Base Protocol V. P.s ............................................................................. 49
MAGICAVP .............................................................................................................. 50
Link Management Interface .............................................................................................. 52
Overview........................................................................................................................... 52
Common Link Interface Primitives.................................................................................. 55

Link Resource 56_

Aircraft Interface..................................................................................................................... 56
Time Format and Synchronization.............................................................................. 56
Central TimeS ou.rc.e …………………………………………………………………57
MAGIC Ground-to-AirTime Servicing.......................................................................... 57
Network Time Protocol (NTP)..................................................................................... 57
Recommended Time-Protocol Implementation..................................................... 57
Aircraft State Information............................................................................................... 58
Configuration and Administration...................................................................................... 58
Service-Provider Versus Airline-Hosted Ground Peer Function .................................. 58
Aircraft-Ground Management ............................................................................................ 59
NETWORKMAINTENANCE, LOGGING, AND SECURITYREQUIREMENTS ......... 60
Accounting Management(AM) Requirement ................................................................ 60
Usage Records .............................................................................................................. 60
Configuration Requirements and Recommendation....................................................... 60
System Clock.................................................................................................................. 60
Computing and Data Storage Resources................................................................... 60
Security ManagementRequirements.................................................................................. 61
Security Event Logging.................................................................................................. 61

ATTACHMENTS
ATTACHMENT 1 CLIENT INTERFACE PROTOCOL DETAILS .............................................. 62
ATTACHMENT 2 MODIFICATIONS TO IEEE 802.21-2008...................................................... 9

APPENDIXES

APPENDIXA ACRONYMS........................................................................................................... 102
APPENDIX B EXTENDEDDIAMETER COMMANDSTRUCTURE ......................................... 105
APPENDIX C GUIDANCE WITH RESPECT TO AIRCRAFT DOMAIN REFERENCE

MODEL..................................................................................................................... 114
APPENDIXD USE CASES .......................................................................................................... 115

V

ARINC PROJECT PAPER 839
TABLE OFCONTENTS

FIGURES
Figure 2-1: MAGIC Functional Concept

( Centralized for AllDomains)

........................................................ 7

Figure 2-2: MAGIC Functional Concept (Federated on Different Domains) ............................................... 8

Figure 2-3: MAGIC Interfaces ................................................................................................................................... 10

Figure 3-1: MAGIC Functions ,

Internal and Managed External , with Interfaces....................................... 15

Figure 4-1: Start and Normal Operation ..................................................................................................................37

Figure 4-2: Change Operations - Change of Session Parameters ............................................................. 38
Figure 4-3: Change Operations - Link Controller Failure .................................................................................39

Interface Architecture ........................................................................ 53
Figure 4-4: MAGIC Link Management
Figure 4-5: Example Architecture Illustrating MIHF Instances ....................................................................... 54

Figure 4-6: IEEE 802.21-2008 MIHF Reference Model .................................................................................... 55

vi

ARINC SPECIFICATION 839-Page 1

1.0 INTRODUCTION

1.0 INTRODUCTION

1.1 Purpose

This standard defines an Internet Protocol(IP)-based communication management
function for data communications called the Manager of Air/Ground Interface
Communications(MAGIC).This standard recognizes that
off-board communication links
terrestrial,Gatelink),and that each link may be provided by a different service
provider.In addition,each link may be available to the aircraft
in specific flight
phases and in specific geographic locations.As the industry continues to
increasingly embrace IP-based communications,there is reason to define an aircraft
routing function that provides standardized access by all aircraft subnets to available
communication links.

the number of available
is expanding(e.g.,broadband satellite,broadband

When an aircraft has only one communication link for IP traffic to ground,the
method to connect that link to the various onboard clients is relatively simple.
However,an Interface Control Document (ICD)is desired with even one link.
Additionally,when multiple off-board communication links are available to an
aircraft,the need to take advantage of each link(in terms of coverage,cost,
availability,etc.) becomes complex and needs to be handled in a standardized
manner to preclude the need for tailoring onboard clients to the specific
characteristics of each available aircraft-ground link.This concept
function of the Communications Management Unit(CMU)in an Aircraft
Communications Addressing and Reporting System (ACARS)-configured aircraft.
Though it has been suggested that an Ethernet port could be added to the CMU
design,the functions involved in routing ACARS messages and IP packets are
vastly different.Combining these functions would be prohibitively complex.

is similar to the

This standard recognizes the presence and availability of multiple subnetworks to
the aircraft.Equipment manufacturers and service providers have developed IP-
based communications methods.For example, ARINC Characteristic 781: Mark 3
Aviation Satellite Communication System defines IP services available over satellite;
ARINC Characteristic 791: Ku/Ka Band Satellite Communications defines an IP
broadband subnetwork providing non-safety services to the aircraft.

This standard has been written to provide both general and specific guidance for
managing multiple types of
IP communication links that may be available on an
aircraft.

1.2 Multi-Phase Development of this Standard

MAGIC is being defined incrementally in this standard over multiple phases.This
document was prepared for phase 1 MAGIC and it considers to the extent possible,
future definitions of MAGIC where capabilities,requirements,and functions are
specifically identified in a future supplement to this standard.The preparation of a
future supplement is expected to take place after phase 1 is completed and as
airline industry determines the need for new capabilities.The activities of both
phases are detailed in the following sections.

1.2.1 Phase 1 Activities

Phase 1 activities include conceptual definition and scope with respect to the
Aircraft Network Services(ANS)reference model,and in particular,the Airline
Information Services(AIS)and Passenger Information and Entertainment Services
(PIES)domains.This specification has been developed with a focus on data flows
with minor hazard and no effect Design Assurance Levels(DALs).

ARINC SPECIFICATION 839-Page 2

The following elements are included in phase 1:

1.0 INTRODUCTION

·

·
·

·
·

·

Segregation principles applicable to aircraft network domains as described in

ARINC Report 821:Aircraft Network Server System(NSS)Functional
Definition and ARINC Specification 664:Aircraft Data Network,Part 5,
Network Domain Characteristics and Interconnection

Interface definition between airborne clients and MAGIC
Interface definition between MAGIC and air/ground communication links
(media independent

interface to a Data Link Module(DLM))

Air/ground communication system interface proxy in MAGIC
Communication systems(i.e.,development of an Interface Control
Document(ICD))
End to end communication management services-functionalities and
interfaces:
o Quality

of Service(QoS),Policy-based Routing(PBR),performance

enhancement
administration

(e.g.,compression),monitoring,logging,accounting,

o Profiling(pre-definable-also for support of

legacy

clients)

o Security

(authentication,authorization,access

control,and filtering)

o Management

information structures,formats,and services

1.2.2

Future Activities

Future activities include a conceptual extension of scope to include the Aircraft
Control(AC)domain.The following elements may be included in the future:

·

·

·
·

·
·

Interface between redundant MAGIC implementations

Particular requirements of avionic systems of higher regulatory awareness
including:

o Higher DALs
o Higher Security Assurance Level
o Additional Certification Aspects

Interface between federated MAGIC implementations
(Optional)Interface to a ground-hosted communication management
function

QoS(such as link optimization)
Security (such Public Key Infrastructure (PKI)management
encryption,signing,etc.)

including

· Wire Level protocol for the Common Link Interface

1.3 Document Organization

This standard is organized into the following sections:

·
·

·

·

Section 1 is the introductory section.
Section 2 describes the MAGIC concept of operations,(i.e.,a summary of
the functionalities offered by the MAGIC service and its terminologies).
Section 3 presents MAGIC functional requirements(i.e.,provides a detailed
specification of the MAGIC service and protocol requirements).

Section 4 details the MAGIC interface definitions.

ARINC SPECIFICATION 839- Page 3

1.0 INTRODUCTION

·

·

Section 5 presents MAGIC network management requirements.

Attachments:
o Attachment 1 describes the protocol details pertaining to the MAGIC

Client

Interface.

o Attachment 2 provides a list of deviations from IEEE 802.21-2008

primitives that are required by the MAGIC service.

o Attachment 3 provides a Protocol

Implementation Conformance

Statement (PICS).An implementer of this standard is encouraged to
complete these PICS in order to identify the requirements that have been
implemented.The PICS are grouped in three parts which address the
different actors within the MAGIC environment.

·

Appendices:
o Appendix A is the acronym list.

o Appendix B provides guidance on the domain reference model.

o Appendix C discusses the role of MAGIC use cases.

1.4 Reference Documents

The following documents contain guidance provisions which,through reference in
this standard,constitute provisions of this standard.The most recent versions apply:
ARINC Specification 664: Aircraft Data Network,Part 2,Ethernet Physical and
Data Link Layer Specification

ARINC Specification 664: Aircraft Data Network,Part 3,Internet-Based Protocols
and Services

ARINC Specification 664: Aircraft Data Network,Part 5,Network Domain
Characteristics and Interconnection
ARINC Characteristic 781:Mark 3 Aviation Satellite Communication System

ARINC Characteristic 791:Aviation Ku-Band and Ka-Band Satellite
Communications System

ARINC Report 811:Commercial Aircraft
Operation and Process Framework

Information Security Concepts of

ARINC Report 821:Aircraft Network Server System(NSS)Functional Definition

ARINC Specification 822: Aircraft/Ground IP Communication

ARINC Specification 830: Aircraft/Ground Information Exchange (AGIE)

ARINC Specification 834:Aircraft Data Interface Function (ADIF)

ARINC Report 842: Guidance for Usage of Digital Certificates

IEEE 802.21-2008:IEEE Standard for Local and Metropolitan Area Networks -Part
21: Media Independent Handover Services

IETF RFC 2246: The TLS Protocol Version 1.0
IETF RFC 2474: Definition of the Differentiated Services Field (DS Field)in the IPv4

and IPv6 Headers

IETF RFC 6733 :Diameter Base Protocol

ARINC SPECIFICATION 839-Page 4

1.0 INTRODUCTION

NIST SP800-53v3, AC-4: Recommended Security Controls for Federal Information
Systems and Organizations

3GPP 23.060 V10. 1.0: 3rd Generation Partnership Project;Technical Specification
Group Services and System Aspects;General Packet Radio Service (GPRS);
Service Description;Stage 2(Release 10)

1.5 Document Terminology

this document,the term"MAGIC service"refers to an implementation of

Throughout
a MAGIC-compliant function in the form of a routing service overlay on an existing
network infrastructure.The terms "MAGIC service,""MAGIC implementation,"and
"MAGIC entity"are equivalent.

1.6 Document Precedence

This standard is written as a companion document to existing ARINC Standards that
define broadband IP data communications on an aircraft.The system-level
are defined in ARINC Standards defining communication links,including satellite
datalinks,terrestrial datalinks,and Gatelink.The airborne routing services for
IP
communication are defined by this standard.

IP links

1.7 Regulatory Approval

All airborne systems must meet applicable regulatory requirements.MAGIC forms a
part of a larger onboard information system.This standard does not and cannot set
forth the specific requirements that the system must meet to be assured of approval.
Such information must be obtained from the appropriate regulatory authority.

2.0 MAGICOVERVIEWAND BENEFITS

ARINCSPECIFICATION 839-Page 5

2.0 MAGIC OVERVIEW AND BENEFITS

This document defines a standard onboard service that provides a harmonized
interface between clients and IP-based air-ground datalinks in an end-to-end
communication context,thereby providing transparent communication services.

2.1 Benefts of MAGIC

MAGIC offers benefits to a variety of stakeholders.This includes a common
standardized interface to ease the integration of multiple applications with the

available off-board communication links so that the links can be used in an efficient
and less costly way.Furthermore,it can provide extended security capabilities and
manage performance.The benefits to stakeholders are described below.

2.1.1 Benefits to Airlines

MAGIC allows airlines to define the access rights and relative priorities of the clients
that use the onboard and air-to-ground network services in a centralized and
efficient way.This enables control over the use of network resources according to
the airlines'business rules and allows the airlines to use a common network
infrastructure to support clients with very different
within and between aircraft domains)without adverse effect.In particular,it allows a
more efficient use of the air-to-ground links within and across multiple aircraft
domains with real-time optimization of the routing according to the offered load and
business rules.

levels of business needs(both

2.1.2 Benefts to Airframe Manufacturers

MAGIC provides a common means to integrate the control features of both onboard
(e.g.,datalink terminal equipment,routing,traffic
network/communication equipment
control,etc.) and client applications and services.This allows a reduction in system
integration cost and a reduction in the time needed to integrate new applications
and datalinks into the aircraft

2.1.3 Benefits to Datalink Equipment Suppliers

MAGIC offers a standard approach to interface the control features of modems and
other datalink equipment
control
supplier is more capable to offer its equipment into the aviation market due to the
reduced integration costs into any aircraft.

interoperability between network components that are MAGIC compliant.A

into the control functions of the network.This allows

2.1.4 Benefits to Avionics Application Suppliers

MAGIC offers a single standard interface to the off-board communication services
through which it can both express its communication needs(e.g.,bandwidth,QoS,
etc.)and be notified in real time of how the network is able to service those needs.
This allows a greater visibility of the off-board communication service-availability to
the avionic applications.A supplier is more capable to offer its application into the
aviation market due to the reduced integration costs into any aircraft.

2.2 Concept of MAGIC

This section describes operational needs,operational behavior,and functionality
that define MAGIC.Furthermore,this document provides MAGIC service
characteristics and the stakeholders'operational needs in a manner that can be
verified by the stakeholders without requiring extensive specialized technical

ARINC SPECIFICATION 839-Page 6

2.0 MAGICOVERVIEWAND BENEFITS

knowledge,but also provides the reader with MAGIC design constraints and their
rationale for those constraints.

2.3 Overall Functional Scope

MAGIC is a network layer management service for off-board communications.It
is
responsible for managing how clients use air-to-ground links.MAGIC is comprised
of functions and interfaces.

2.3.1

Functions for which MAGIC is Responsible
Specifically,MAGIC is responsible for:

· Access control between clients and off-board links by using Authentication,

Accounting,and Authorization Services(AAA)

· Managing the routers to the off board links

·

·

·

·

Opening and closing air-to-ground links via the link controller

Coordinating access to off-board communications according to
administrative policies

Providing standardized interfaces to clients

Providing standardized interfaces to the link controllers

· Managing routing policies (including QoS,queuing,etc.)
·

Facilitating IP-based traffic between aircraft and multiple ground clients
belonging to the different aircraft system domains

2.3.2 Functions for which MAGIC is Not Responsible

MAGIC is not responsible for:

·

·

The routing function itself within its perimeter

The firewalling function itself within its perimeter

Overruling existing link management functions

·
· Activating or deactivating the radio belonging to a communication system

2.4 Architectural Overview

This standard addresses two system architectures for MAGIC which may be
centralized or distributed as described in the following two scenarios.

2.4.1 Centralized MAGIC

IP
A centralized MAGIC is considered as one MAGIC service that manages external
links for clients in multiple domains as shown in Figure 2-1.This centralized MAGIC
service can also be seen as a logical combined overview of multiple MAGIC service
implementations.

ARINC SPECIFICATION 839-Page 7

2.0 MAGIC OVERVIEWAND BENEFITS

Satelite
NetworkA

Satellite

Network B

AC-Domain

AIS-Domaln

PIES-Domain

POD-Domain

Crew
Appr

SaselneB

MAGIC

5ervices

Satelite A
Modem2

Service

Passengu
Device

Wned
Getgink

Celular
Calelink

WF
Gatnirk

Terestnial
Mooen2

Crew
Appe

Cellular

Gatelink

A

Cellular
Service
Provider

Satellite

Service
Provider

A

WiFi
Gatelink

Airport

Airline

Terrestrial
Service
Provider

M

Satelite
Service

Provider

B

Figure 2-1:MAGIC Functional Concept

(Centralized for All Domains)

COMMENTARY

A modem is part of a communications system comprising the radio
functions,the service provider network,a management
(called link controller)offering an interface that can be managed by
MAGIC,and a user data gateway that
is used by the network to
forward the user data to/from based on the actual network rules.

function

2.4.2

Federated MAGIC

The centralized MAGIC service shown in Figure 2-2 can also be seen as a logical
view of all
installed federated MAGIC instances that are managing connectivity for
its clients.Figure 2-2 shows how this logical or centralized approach can be broken
down into several
communicating with each other via a MAGIC internal management

federated MAGIC services located in different domains that are

interface.

ARINC SPECIFICATION 839-Page 8

2.0 MAGIC OVERVIEW AND BENEFITS

Satelite
NetworkA

Satellite

Network B

AC-Domain

AIS-Domaln

PIES-Domain

POD-Domain

Crew
Appr

MAGIC

MAGIC

Sabele B
Modem

Sateli地A
Modem2

q
Celular
Galelink

WF
Gatnirk

Wred
Gabelink

Terestnial
Mooen

Ap

Passenge

Service

Passengur
Device

WiFi
Gatelink

Cellular

Gatelink

A

Satellite

Service

Provider

A

Cellular
Service
Provider

wreod
Galelnk

Airport

Airline

Terrestrial

Service
Provider

M

Satelite
Service

Provider

B

Figure 2-2:MAGIC Functional Concept

(Federated on Different Domains)

COMMENTARY
is possible to have multiple federated MAGIC services within one

It
domain.
The ground systems are not part of the MAGIC definition;however,
they may be required to interface with the MAGIC functions on the
aircraft.

2.5 High-Level Overview

The basic operation of MAGIC(in a single domain for clarity)can be described as

follows:

· When they are started,MAGIC-aware clients register with their

local MAGIC

·

function.They provide MAGIC with a client profile that describes their
communication needs.
Air-to-ground link controllers register with the local MAGIC function when it is
powered up.This provides MAGIC with link profiles that describe service
capabilities.

· When a link controller establishes a connection to the ground it notifies its
local MAGIC function of the actual capability it can now offer.MAGIC

2.0 MAGIC OVERVIEW AND BENEFITS

ARINC SPECIFICATION 839-Page 9

processes this information to determine which clients should be allowed to
use the service,based on the profile parameter and administrative policies.
MAGIC configures the network so that the selected clients can communicate
over the selected link(i.e.,establishing routes,enabling access control
resources,etc.)and notifies the client that

it can begin using the resources.

to

·

·

If a non-MAGIC-aware client starts,it attempts to use the network resources.
As MAGIC has not configured the network resources to allow this client to
communicate to the ground,it
is initially unsuccessful (i.e.,its packets are
dropped).MAGIC,however,assigns this client a default profile and
if possible(i.e.,configure routing,access control,etc.),so
accommodates it
that the client's traffic is not dropped.This happens transparently to the non-
MAGIC-aware client.
As air-to-ground resources vary over time,MAGIC compares available client
and link profiles to determine which traffic should use which links(following
administrative policies)and setup network resources accordingly.

In all of this,MAGIC ensures that the available network and air-to-ground resources
are used most effectively,i.e.,that the service level awarded to high priority data
flows is not degraded by lower priority flows and that administrative policies placing
constraints on usage of datalinks are respected.

Operation across domains requires MAGIC instances to communicate among
themselves to determine which clients should use which air-to-ground links.This
requires sharing information about client and link profiles among themselves and
determining an optimal resource allocation solution across the domains (abiding by
administrative polices)

The value of the MAGIC service is realized by its capability to dynamically allocate
available bandwidth among shared links in accordance with the prescribed
requirements of the clients,airframe manufacturers and the airlines.MAGIC-aware
clients can dynamically utilize the available bandwidth.Further,while MAGIC needs
to know of the non-MAGIC-aware allocations to calculate the available bandwidth
for MAGIC clients,the non-aware clients will have a fixed bandwidth allocation.
In addition,the present link controllers would need to have additional capabilities to
work with MAGIC.In the future,the link controller may include a Media Independent
Interface(MII)and/or a Data Link Module(DLM)(IEEE 802.21-2008)that has this
capability.

Figure 2-3 operationally illustrates the management and data flows through a
MAGIC-enabled network,and the associated five interfaces,including a MAGIC-to-
MAGIC(i.e.,dual MAGIC services)interface.The interfaces are further described in
Sections 3 and 4 of this document,as detailed in the following paragraphs.

ARINC SPECIFICATION 839-Page 10

2.0 MAGIC OVERVIEW AND BENEFITS

MAGIC

MAGIC to.MAGIC
Interface

(currently undefineo)

Other
MAGIC

MAGIC

Management

Client
Interface

LAN
Interface

Common
Link
Interface

Client

(MAGIC.Aware)

Client
(Non.MAGICAware)

③

LAN

CommLink-
MAGIC Aware

Link

Control

Modem

Data

Link Module

Comm Link-
Non MAGIC
Link
Control

Modem

Other

Client

Other
LAN

Other
Comm Link

Management Flow
Data Flow
Not MAGICdefined

MAGIC Interface
Interface not defined by MAGIC

Figure 2-3:MAGIC Interfaces

The five MAGIC interfaces are described in this standard as follows.

1.Conceptual MAGIC Overview of the Client
Section 3 in the context of the Client
Section 4 in the context of Diameter(a widely used Authentication,
Accounting(AAA)protocol).
Authorization,and

Interface is further discussed in
Interface Controller(CIC)and defined in

2.Common Link Interface is further discussed in the context of DLM in Section
3,link and network management are discussed in Section 3, and some
elements are defined in Section 4 in the context of Diameter.

3.Underlying Local Area Network (LAN)Interface is discussed in Section 3
(The management interface to the underlying network is described at the
logical
etc.).

level because the interface depends on the chosen routers,firewalls,

4.MAGIC-to-MAGIC Interface requires passing on link status,etc.,and access
between multiple MAGIC services.Though not defined in this specification,
two independent MAGIC services may communicate using the Client
Interface Protocol.
Link

5.Media-specific

Interface.

2.6 MAGIC Development Phases

The development of this standard is divided into two phases.Selected domains are
addressed in each phase.

This standard identifies a phased approach to integrating all domains that comprise
the Aircraft Data Network(ADN)reference model.

2.6.1 Development Phase 1

Phase 1 addresses the Airline Information Services (AIS)domain and the
Passenger

Information and Entertainment Services (PIES)domain as defined in

ARINC SPECIFICATION 839-Page 11

2.0 MAGIC OVERVIEW AND BENEFITS

ARINC Specification 664: Aircraft Data Network,Part 5,Network Domain
Characteristics Interconnection.
This standard targets retrofit
installations as well as new developments.This
standard establishes system-level and functional-level requirements that are
independent of any operating system or hardware.

In particular,the purpose of this standard is to define the following items:

·

·

·
·

·

·

·

·

·

Segregation principles between domains under consideration as described
in ARINC Specification 664,Part 5,i.e.,focusing on the AIS and PIES
domains.For phase 1,all systems and hardware are assumed to be minor
or no hazard or they may be mitigated to minor or no hazard.

Interface between distributed(federated)MAGIC services.

Interface of clients to and from MAGIC.
Interface of MAGIC to/from other aircraft-based aircraft-ground
communication systems via a media independent interface by considering of
a retro-and forward-fit capable interface proxy within MAGIC.
Communication Management Services,functionalities,and interfaces for at
least QoS,PBR,Performance Enhancement
compression),Monitoring and Logging,Accounting,and Administration.

(e.g.,link optimization,

Communication handover management functions necessary to maintain the
interlink

communication network internal handover management or external
handovers.
Security functions for the front-end (client)and back-end (communication
link)interfaces.

Definition of data structures,formats,and services relating to the protocols.

Definition of client profiles (also for support of legacy clients).

This concept development includes the means to provide a future expansion to
support the AC domain in the second phase.In the first phase,this requires the
treatment of segregation principles between domains under special scrutiny by the
Civil Aviation Authorities (CAAs),as well as other certification and accreditation
considerations.

2.6.2 Development of Future Supplements

The development of future supplements is considered a future growth area.This
may include guidance for the AC domain communications over IP.In particular,
future supplements may define the following:

·

·

·

Introduction of an interface between two or more redundant MAGIC
implementations within a single domain.
Development of the requirements for avionic systems of higher development
assurance levels.

Interface to Ground-to-Aircraft Communication Management Function.This
interface may include onboard prioritization and QoS information to allow the
ground to actualize the queuing and prioritization schema for the ground-to-
aircraft direction(i.e.,development of an ICD between MAGIC and the
ground communications service provider),receipt of ground information
(e.g.,available bandwidth)to include it
and allow complementary ground routing and prioritization.

into the onboard routing decisions,

ARINC SPECIFICATION 839-Page 12

2.0 MAGICOVERVIEWAND BENEFITS

2.7 Management and Data Flows

The value of the MAGIC service is achieved at an aggregate level where multiple
clients and multiple off-board communication links exist.Its value is also subject to
be seen over time as airlines improve their business operations by leveraging
evolving off-board communications service offerings and maximizing link usage and
efficiency.MAGIC provides reduced off-board link communications costs and
reduced development and integration costs for new clients and communications
links.The distillation of Management and Data Flows in this section encompasses
one level of abstraction above use cases and describes MAGIC operations at the
aggregate level.

The approach taken for this section is to list the key stakeholders that may have a
business interest
support to provide the business benefits that clearly state the stakeholder
expectations.This section focuses on the basic data flow operations and
fundamental aspects of management operations flow.

in MAGIC,describe the major operations that MAGIC must

There are four key stakeholders involved in MAGIC development,implementation,
and operation:

1.Airlines are the key stakeholders and include:flight crew,cabin crew,

maintenance,dispatch,and

passengers.
2.Developers of onboard client services that
3.Communication link providers(both equipment providers and service

require off-board links.

providers).

4.Aircraft manufacturers and/or LAN providers that develop the onboard

computing and networking equipment and functions.

Design considerations included in the initial operations selection are:

1.Routing IP data between airplane and ground systems.

2.Utilizing off-board communications

links.

3.Managing cost of off-board services.

4.Reducing onboard system complexity,cost,and weight.
5.Reducing integration and re-integration costs of adding clients and

communication links.

6.Minimizing cost of developing new MAGIC-aware communication links that

can function across industry.

7.Minimizing cost of developing new MAGIC-aware clients that can function

across industry.

8.Identifying MAGIC-to-MAGIC communications in a federated MAGIC

environment.

2.8 Functional Description of MAGIC Operations

Each MAGIC operation is formatted to identify the stakeholder,situation,objectives,
and expectations.The operations are broken into two general classifications:Data
Flow Operations(DFO)and Management Flow Operations(MFO).DFO
encompasses those MAGIC functions that provide benefits regarding IP traffic
flowing from clients to communication links.MFO illustrates how MAGIC minimizes
costs of development,integration,and operation of off-board communications-based
functions.

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

ARINC SPECIFICATION 839-Page 13

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

This section provides a top-level functional overview of MAGIC and describes the
detailed functional aspects of each MAGIC component.Also provided is an
overview of the MAGIC interfaces to the connected aircraft systems and ground
entities.

3.1 Top-Level Functional Requirements

A MAGIC service must meet the following top-level requirements:

1.A MAGIC service shall provide clients with a media independent access

control mechanism to the off-board IP communication links.

2.A MAGIC service shall provide a standard communications management

interface for IP-based communication to ground systems.

3.A MAGIC service shall manage IP communication links in the domain it

is

hosted,but also optionally in a connected domain.

4.A MAGIC service shall manage IP communication link access and IP traffic

routing based on airline configurable policy.

5.A MAGIC service shall manage authorization,authentication,and accounting
of the IP communication link usage based on the profiles of the different
clients by preventing unauthorized attempts (i.e.,access control)to access
MAGIC services by reporting anomalies.

6.A MAGIC service shallprovide communication status information to clients.
7.A MAGIC service shall provide QoS mechanisms to enable clients to request

traffic prioritization.

8.A MAGIC service shall allow client

level

tunneling,such as Virtual Private

Network (VPN).

9.A MAGIC service shall support the security controls that are required by the

governing network security architecture.

10.A MAGIC service shall operate in the airplane LAN,including the off-board

communications and the associated link security architecture.

COMMENTARY

It is assumed that the functions and systems being managed by
MAGIC will provide the required interfaces for MAGIC to function
(e.g.,a network router to allow MAGIC to specify a "per flow"routing
level and
table).These interfaces are only addressed on a functional
they are not specified down to the wire-level protocols since the
hardware chosen may already have interfaces already in place that
would need to be used by the specific MAGIC implementation.

3 . 2 Top- Level Functional and Interface Description

A MAGIC implementation shall perform the functions defined in this standard
independent of any chosen architecture.

This section provides a more detailed description of the following items:

● MAGIC internal functions
MAGIC-managed external
·

functions

● MAGIC interfaces to external entities

ARINC SPECIFICATION 839-Page 14

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

·
·

High-Level Functional Description
High-Level

Interface Description

3.2.1 MAGIC Internal Functions

These are the high-level functions within MAGIC itself that are needed to operate
and manage the existing communication links on behalf of the connected clients.

The MAGIC internal functions are:

Central Management (CM)including profiles

·
● Administration function
·

Synchronization function for ground-to-aircraft prioritization

· Client

Interface Controller (CIC)

·

Aircraft Parameter Retrieval Function

3.2.2 MAGIC-Managed External Functions

MAGIC-managed external functions and components are required at the network
level to manage user data.While these functions and components are not part of
the MAGIC definition,they are managed by MAGIC to ensure that the user data are
processed correctly.The management of such components is only described at a
functional
level without providing a detailed protocol stack,as a supplier can choose
Commercial-Off-The-Shelf (COTS)software that provides the desired management
interfaces and stacks to be used.

· Routing Function

·
·

·

·

Firewalling Function
Packet scheduling(managing queues to deliver prioritization and
multiplexing)
Traffic shaping(e.g.,flow rate limiting)

Communication Links(may be managed by DLM)

A DLM is a MAGIC-compliant link controller driver that may not be

COMMENTARY

available as COTS software.

3.2.3 MAGIC Interfaces to External Entities

These external

interfaces are needed to interact with external entities,including

clients or link controllers.These interfaces are needed to:

Offer functionalities to clients

·
· Manage the modems via the link controller interface

·
·

Synchronize with other MAGIC instances
Synchronize with ground-hosted management functions

This specification details and describes these interfaces down to protocol
these interfaces are communicating to external entities.Figure 3-1 provides an
overview of the primary functions that are integral to a MAGIC implementation and
also shows the MAGIC interfaces to the external components.

level,as

3.0 FUNCTIONAL REQUIREMENTSOF MAGIC

ARINC SPECIFICATION 839-Page 15

MAG IC

Client
Interface
Controller

Client
Profiles

Administration
Function

Central
ManagementL

Ground to Alrcraft
Communicasion
Management
Function

Param

Retreva
Function

Link

Coniroller
Prohies

Central
Polcy Prolie

Network

(Routing

&Firewalling)

Mawon

Mumplxer
Schodulend

Network
(Queuing,Shapng)

LM

DLM

Comm.Link

Controller
#2

omm.Link
Controller
加

MAGIC intemal Functions

Protected Management

Interface

Mangement Gateway/Tunnel

Unproctected Management Interface

MAGIC managed eoctemalfunctions

Cliert User Data

Date Gateway/Tunnel

9e oncyeet s

lhaentme r eirho oy seme

Client

intertace Protocol

Encryplon Endpoint

Figure 3-1:MAGIC Functions,Internal and Managed External,with Interfaces

3.2.4 High-Level Functional Description

This section contains a brief description of MAGIC internal
managed external

functions.

functions and MAGIC-

ARINC SPECIFICATION 839-Page 16

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

3.2.4.1 Functional Description of MAGIC Internal Functions

3.2.4.1.1 Central Management

Central Management(CM)encompasses all MAGIC processing.MAGIC makes
decisions on link selection based on a set of various profile types.These profiles are
loaded into the MAGIC functionality to create an internal traffic management policy.
The following profile types are defined:

·

·

·

Client Profiles

Datalink Profiles

Central Policy Profile

3.2.4.1.1.1 Client Profiles

Client profiles define the needs of the clients.There is one profile per client.There
are two different types of client profiles:

·

·

Static Profiles for non-MAGIC-aware clients with or without QoS needs.

Dynamic Profiles for MAGIC-aware clients with or without QoS needs.

3.2.4.1.1.1.1 Profile for Non-MAGIC-Aware Clients

The first type of client profile is the static client profile for non-MAGIC-aware clients,
Interface).These clients are not aware
(i.e.,those that do not use the MAGIC Client
that there is a communication management service present.Non-MAGIC-aware
clients send IP packets as if they were directly connected to the Internet.

MAGIC must consider the presence of traffic from all clients in its overall
communication strategy.

The data parameters are the same as those of the dynamic profiles with the
restriction that a client profile contains non-negotiable static values(e.g., a fixed
maximum best effort throughput).

MAGIC shall provide a mechanism that allows detection of this traffic and to map
the traffic to a Static Client Profile.

Client-specific profiles enables Magic to recognize traffic from certain clients and
handle their

traffic differently(e.g.,assign priority).

COMMENTARY
It is possible to define a default static client profile for traffic that is not
recognizable by MAGIC.This is called "best effort"service.A
possible use of a "best effort"service is a new client that is installed
on an EFB but not yet authorized with MAGIC.There are security
implications that are required to be analyzed and these may affect
routing decisions.

3.2.4.1.1.1.2 Profile for MAGIC-Aware Clients

The second type of client profile is the Dynamic Client profile,and it is associated
with MAGIC-aware clients.This profile defines the boundaries of the values a client
can request at the Client
are a set of parameters that establish the range of values that can be requested.
The profile defines the source and potential destinations that are allowed,the
maximum throughput that the client can request from MAGIC,and the type of
is necessary(e.g.,best effort or guaranteed throughput).Another
service that

Interface Controller (CIC)via the interface.The boundaries

ARINC SPECIFICATION 839-Page 17

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

parameter defines the flight phase in which the client is allowed to communicate
(e.g.,"only at the gate”or "in-flight"or “cruise phase above 30,000 feet").The profile
also defines the access rights for the client.There may be some functions (like
receiving call data records to prepare a client based bill)that only should be
provided to dedicated clients.

3.2.4.1.1.2 Datalink Profiles

These profiles define all the necessary parameters that are needed to set up a
datalink via a link controller.There is at least one profile per link controller,but it
may be necessary to have more than one profile for link controllers that are able to
communicate via different service providers.A profile should be provided for each
DLM and the associated service provider.
Datalink profiles shall be defined for each phase of flight in which the link controller
is active and can be used.For example,this may include the maximum throughput
and mapping of the communication types of the link controller with the internal type
of services (like the mapping of SwiftBroadband (SBB)-Background class to the
MAGIC Best-Effort

type).

The profile defines the maximum available channels that can be established at the
link controller and the credentials that are needed to register at the network.The
profile may also include location parameters for communication systems that may
have to operate differently in dedicated outstations or at the home-base.

3.2.4.1.1.3 Central Policy Profile

This profile defines the basic behavior of MAGIC in terms of Client Profiles and
Datalink Profiles.This profile defines whether MAGIC forwards the traffic based on
availability without considering cost or if a less expensive link should be preferred to
save cost.This profile may contain parameters about the interfaces of the clients
and also of the communication means that are included in the management
decisions.

3.2.4.1.2 Administration Function

loading new configurations into the system.For example,it may allow

The Administration Function allows loading of the reconfigured parts of MAGIC
without
changing some basic rules according to the Central Policy Profile.A detailed list of
the MAGIC administration capabilities is provided in Section 4.4 below.

3.2.4.1.3 Ground-to-Aircraft Communication Management Function

for each link(located in a Service Provider environment)with the

The Ground-to-Aircraft Communication Management Function is responsible for
sharing the QoS parameter and link priorities with the ground.It notifies the ground
counterpart
necessary parameters needed to establish priority and QoS rules between ground
and aircraft to ensure an optimized communication based on the actual needs.The
protocol to exchange information between the MAGIC services located in the aircraft
and a ground interface.This interface is currently out of scope of this document.

3.2.4.1.4 Client

Interface Controller

The Client

Interface Controller(CIC)is a module that provides the interface to the

clients.

ARINC SPECIFICATION 839-Page 18

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

As MAGIC operates on different aircraft network domains,it must support
established security measures in accordance with the domains to which the security
measures belong.

The interface between this controller and the central management must be robust
and able to protect the internal MAGIC functions against potential security threats.
This means that this module provides two levels of defense.The first level
interface to the clients itself that only allows the commands and requests defined in
that protocol stack and the second level
is the secure interface to the CM.The
detailed protocol stack and its functionalities are defined in Section 4.1 and in
Attachment 1.

is the

COMMENTARY

If the second interface requires a protocol that is different than the
protocol presently defined,clients at the front-end side will need to be
analyzed further.

3.2.4.2 Functional Description of External Functions Managed by MAGIC

MAGIC interacts with other functions that are necessary to enable the
communication between the aircraft and the ground.These functions are not directly
part of MAGIC itself but are managed by MAGIC.This section provides a brief
overview of the external functions managed by MAGIC.

3.2.4.2.1 Network Queuing,Prioritization

The Prioritization Function detects each IP-packet entering the system and validates
it with the assignments negotiated within the interface calls or with the static
assignments in the profiles.

Based on this validation,the packet is forwarded to the Queuing Function provided
by the network based on the availability of communication links that are able to
serve the needed QoS schedules,and places it into the appropriate queue
according to the network (scheduler/multiplexer).

This function is responsible to control the flow of information between designated
sources and destinations based on the characteristics of the information and/or the
information path.Specific examples of flow control enforcement can be found in
boundary protection devices
(e.g.,proxies,gateways,guards,encrypted tunnels,
firewalls,and routers)that employ rule sets or establish configuration settings that
restrict information system services or provide a packet filtering capability
(Reference NIST SP800-53v3,AC-4).

COMMENTARY

The Prioritization and Queuing Functions described as part of the
multi-domain concept logically belong to the domain where the
originating clients are hosted.The hosting of this function may be
done on the same physical unit,as part of a virtualization concept,or

on a dedicated unit within the same domain.These functions must be
protected against attacks independent of the hosting.They have to
be totally transparent,as the clients and the hosting environment
need only process packets addressed to the ground.In addition,the

interface between these functions and the CM must be secure at the
second level of defense.

3.0 FUNCTIONAL REQUREMENTSOF MAGIC

ARINCSPECIFICATION 839-Page 19

3.2.4.2.2 Firewalling Function

The Firewalling Function has two roles.First,the Firewalling Function blocks all
traffic that may be forwarded between aircraft and ground.This Firewallfilters traffic
in both directions.Second,this function must open and close dedicated ports in the
forwarding chain based on the management information provided from MAGIC.

COMMENTARY
This function does not belong to MAGIC,but it should be in the same
aircraft domain as the CM (if possible,within the same unit),because
all connected link controllers in a domain will

interface to it.

3.2.4.2.3 Routing Function

The Routing Function is able to decide where to forward the IP packets.This
function knows the correct network interface to be used based on its routing table.
The management of the routing table is part of MAGIC(i.e.,activation,deactivation
or modification of routing rules),but the routing itself is part of the underlying LAN
equipment.

COMMENTARY

This function does not belong to MAGIC but should be in the same
logical domain as the CM (if possible,within the same unit),because
all connected link controllers in a domain will

interface to it.

3.2.4.2.4 Data Link Modules

Data Link Modules(DLMs)are similar
to modem drivers.They are responsible for
translating the media independent commands received from the CM to the media
dependent commands of the physical communication means.As a short term
solution,these DLMs can be hosted as small wrapping software modules on the unit
also hosting MAGIC.A long term vision should be that the communication links
interface.A DLM activates and deactivates
directly provide the media independent
its gateway based on the configuration in the DLM profiles.A DLM negotiates with
the CM its capabilities and the functions that can be supported so that MAGIC is
aware of the exact capabilities of the communication link.

COMMENTARY

This function does not belong to MAGIC but should be in the same
logical domain as the CM (if possible,within the same unit),because
all connected link controllers in a domain will

interface to it.

3.2.5 High-Level

Interface Description

MAGIC previously described different types of interfaces for different functions.On
one hand,there are interfaces that are used to exchange information within MAGIC
internally.On the other hand,there are also functions that
and front-end functions outside of the MAGIC perimeter.

interact with back-end

Most of the interfaces are between the CM and the other functions,but there are
also some additional

interfaces to the external clients in the following sections.

3.2.5.1 Client Interface Controller to Clients

The
dom

A
n

G
Th

IC
e

-awar
entry

ie
l
c
e
n
i
o
p

n
t

ts
for

are
th e

nec
n
co
ents
i
c l

te
i

v
c a

a
ia
ed
ll

n

terf
in
ent
li

C

ce
a
nt
I

t
e

o
rfa

th
c

e
e

C
Pr

IC w
otoc

i
h
it
l
o

t
ha
in the
P )
I

ARINC SPECIFICATION 839-Page 20

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

Figure 3-1.It is shown as interface 0. This interface has a binding to the diameter
protocol to authorize with MAGIC,to request communication capacities at MAGIC,
to change the properties of the already requested communication link(like
increasing the throughput needs),to close the communication,to request status
information about the communication means mapping to their profiles,etc.

This interface must be a network-based protocol stack that can be easily extended
with new functions without a redesign of the complete interface.

It should include a capability to map the functions offered by MAGIC with the
functions needed by the clients,so that a client does not send commands or
requests that is not supported in a given implementation or environment.

3.2.5.2 Client

Interface Controller to Communication Management Function

interfaces.It

is shown as interface ②in Figure 3-1.

The interface between the CIC and the CM involves the same functionalities as the
CIP-to-client
As part of the multi-domain concept with a MAGIC service operating on more than
one domain,this interface may have to be protected to secure the core MAGIC
is
functions as well as the higher trusted domain.For example,this requirement
necessary to interface with the PIES domain in parallel to the AIS domain since
there may be passengers trying to access the system to communicate without
authorization.If such persons are able to break the CIC or even to completely
disable it,the CM and the connected domains should not be impacted at all.

3.2.5.3 Communication Management Function to QoS-Engine

The interface between the CM and the Queuing,Prioritization,and Encryption
Engine (QoS-Engine)is needed to match the detected IP packets with the rules
established by the CM.It will prioritize the packets in the correct way and to map the
data streams of the non-MAGIC-aware clients to the static profiles.This information
can also be used to manage the other functions in MAGIC.The interface is shown
as interface ③in Figure 3-1.
Because the interface may connect functions hosted in different domains,it must be
secure and protected against potential threats to ensure that the core functionality of
MAGIC is not affected by an attack on a subsystem with which the MAGIC CM
interfaces.

The interface must be made secure according to the requirements of the highest

certifiable domain.

The QoS-Engine in a next step is based on the management information received
from the CM able to establish (encrypted or non-encrypted)gateways to the ground.
These data gateways are shown as interface ⑨in Figure 3-1.

3.2.5.4 Communication Management Function to Data Link Modules

The interface between CM and the DLMs offers a media independent interface to
manage the communication means.It is shown as interface ④ in Figure 3-1 and
provides a command set valid for alltypes of communication link controllers.It must
be designed in a way that it can easily be extended with new functionalities to allow
the management of new link controller types without redesigning the complete
interface.This interface needs to have a capability exchange mechanism to allow
MAGIC to know how and by which messages and commands the link controller can
be controlled.Generally,this interface should provide a minimum set of functions to
open and close a link,to request and release bandwidth,to request or change

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

dedicated QoS capabilities at the network and to request status information about

ARINC SPECIFICATION 839-Page 21

the link and the associated network.

3.2.5.5 Central Management to Network (Routing and Firewalling)

The interface between the central management and the network is needed to
update the rules in the network components responsible for the forwarding of the
data to the correct network segment or gateway.This update includes updating the
Firewall rules to enable or block complete flows as well as routing rules to set the
forwarding rules according to the actual situation.This update includes choosing the
correct gateway,setting quotas rules,etc.This interface is shown as interface ⑤ in
Figure 3-1.

3.2.5.6 Onboard to Ground-Hosted Management Function Information Sharing

The interface between the Ground-Hosted Management Function(GHMF)is out of
scope of this document.It may be defined in a future supplement.

Its intention is to provide the mechanisms to exchange the priority and QoS
information assigned in the aircraft with the ground counterpart (that potentially is
located in the service provider network).This interface is shown as interface ⑥in
Figure 3-1 and must be designed to accept a capability request when MAGIC sends
the request to the ground network.This interface is only established if the interface
can be supported by the ground;otherwise,MAGIC will consider the ground as a
best effort system for the data streams that are sent from ground to the aircraft.This
mechanism is needed because not all communication means and the associated
networks support such interfaces.For example,it cannot be expected that each
Global System for Mobile Communications(GSM)service provider changes its
ground infrastructure to support this kind of interface.The aim of this interface is
mainly to optimize the communication via expensive links with a high round trip time
(like satellite links).An example would be a low priority File Transfer Protocol(FTP)
request of a passenger from blocking a high priority aircraft or emergency function.
The interface should be secured to avoid manipulation by unauthorized parties.

3.2.5.7 Aircraft Parameter Retrieval

Interface

MAGIC needs some parameter from the aircraft to make accurate decisions.
Therefore,the CM does have a subcomponent that
parameters.This interface is not defined on a wire level since there is currently no
common way to get such parameters from different aircraft types.This specification
defines below some parameters that are helpful for MAGIC to make its decision,but
it does not specify how the aircraft is delivering such parameters.This interface is
shown as interface ⑦ in Figure 3-1.

is responsible to retrieve aircraft

3.2.5.8 Data Link Modules to Link Controller

The media dependent interface between the DLMs and the link controller of the
communication means is beyond the scope of this standard,as it is defined on a per
link controller basis (e.g.,ARINC Characteristic 781,Attachment 5).This
interface is shown as interface ⑩in Figure 3-1.
management

3.2.5.9 Federated MAGIC to Federated MAGIC

Another interface,which currently is not shown in Figure 3-1,is responsible to
synchronize multiple “federated”MAGIC entities.The need of a dedicated
synchronization protocol

is not presently defined.This is the subject of further

ARINC SPECIFICATION 839-Page 22

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

discussions,since this synchronization protocol may be done as part of the following
already existing interfaces:

· Client

interface(shown as interface 0 in Figure 3-1)by defining a DLM that
is able to forward client requests to another MAGIC entity in case it cannot
provide the needed resources itself.The benefit of this mechanism is that
each client may interface to one federated MAGIC entity without knowing
who is responsible for the management of the chosen link.

· Common link interface to the link controllers/DLMs(interface ④ in

Figure 3-1)by using the capabilities of the IEEE 802.21 framework for media
independent handover services.

Both versions provide a centralized data communications resource where each
federated MAGIC entity can request or offer resources from or to other federated
MAGIC entities to provide the maximum effective and efficient communication
throughput at any time.This synchronization interface has also the benefit that only

the federated MAGIC needs to know its own "non-MAGIC-aware"clients as it can
request the needed resources dynamically at the other MAGIC entities via the
interface to the other MAGIC entities,if it cannot provide the capacity at the time,
when the traffic appears at the incoming data interface of its own domain.

3.2.5.10 User Data Interface

The User Data interface is used by the clients to forward packets to the ground and
to receive packets originated from the ground.This applies to both types of clients
(MAGIC-aware and also non-MAGIC-aware clients).The primary difference
between these types of clients is that the non-MAGIC-aware clients only try to send
data to the ground without knowing if a route via a communication link is already
established or not.MAGIC-aware clients only try to send data via this interface,
when MAGIC grants the transmission via the client interface and establishes a route
via a communication link.

This interface is totally transparent to the clients.It is part of the routing
functionalities within the domains and detects all packets and maps them to
appropriate profiles and to the actual communication rules.

This interface is shown as interface 8 in Figure 3-1.

3.3 Policy Considerations

MAGIC is a manager of aircraft
IP communication that must operate worldwide.
Therefore,it is necessary to provide all functions that meet the requirements of

countries in which the aircraft is operating.

In addition,MAGIC must operate in accordance with that country's policies where
the aircraft crosses the airspace of that country.

These regulatory aspects and business aspects drive requirements into the policy
system of MAGIC.

MAGIC shall support additional policy needs driven by the airlines,as different
airlines may have different communication policies.

COMMENTARY

The airlines'policies may include the requirement to support every
passenger communication independent of the currently available
links and the associated costs.There may be some airlines that want

ARINC SPECIFICATION 839-Page 23

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

to prioritize the operational traffic from the passenger traffic to
operate the aircraft most efficiently and other airlines that want to
provide the passenger with the best available communication
services at each time.There may be airlines that do not provide
passengers with communication during the night to ensure that
passengers are not disturbing one another.Other airlines may only
want to use the least expensive link that is available at each time to
save cost.Other systems may be allowed to communicate only if the
aircraft is reaching a dedicated altitude or during specific flight
phases.These conditions may change from airline to airline and in
accordance to local regulations.

related traffic that,under normal conditions,should be

Aircraft
encrypted and secured against threats may have to be transmitted
without encryption based on the geographical position.This traffic
may cause redirection of traffic to another ground service station to
ensure legal
United States or China).

interception of traffic(e.g.,bypassing the airspace of the

3.4 Priority Model

When the traffic load on the air ground network is lower than the network's capacity,
MAGIC's role is to manage access control and routing(i.e.,assigning each flow to
the appropriate air-to-ground link).

When the demand exceeds the capacity(momentarily or continuously),the network
may experience congestion.MAGIC can manage the priorities between different
flows to ensure adequate quality of service is selectively maintained for the most
important flows.MAGIC determines which client flows should be protected (i.e.,
maintain the QoS they receive),which flows should experience degraded service,
and which clients should stop transmitting (only possible for MAGIC-aware clients).
It is noted that MAGIC's role is additionally complicated by the fact that all air-to-
ground links may not be suitable for all flows,either due to mismatches between link
and flow characteristics(e.g.,different safety
levels)or commercial constraints(e.g.,
cost).

MAGIC determines and enforces the priorities between traffic flows on the basis of:

·

·

·

·

An administratively set table of priorities,characterizing each flow from each
client (different flows from a given client may have different priorities).The
table also specifies what priority to be given to non-MAGIC-aware clients
using static client profiles.
The link profiles,which define the air-to-ground capacity and service
characteristics.

The dynamic link capacity and status.

The client profiles,which define the desired demand on the network from
clients as well as their service level

(QoS,priority,regulatory,etc.).

· Network load monitoring.

MAGIC uses these inputs to determine the optimal way of meeting the demands in
accordance with administrative policies.MAGIC configures QoS management
functions to dynamically prioritize traffic.

ARINC SPECIFICATION 839-Page 24

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

MAGIC shall protect the throughput of higher priority client flows from lower priority
traffic in accordance with the administrative policy.
MAGIC shall ensure the suitability of the air-to-ground link selected for each client
request
requirements (e.g.,regulatory,security,etc.),as defined in
to service their
the administrative policy.

MAGIC shall shape each flow to not exceed the maximum allowed throughput
defined in its client profile during periods of congestion.

COMMENTARY

How MAGIC determines the optimal configuration for the QoS

mechanisms is implementation specific.

Caution should be used when defining the priorities in a set of client
profiles to insure the expected behavior of each individual client(e. g. ,
a high priority default should not pre-empt other required traffic).

3.4.1 MAGIC Profile Example Based on AGIE Prioritized Path-Selection

For implementations where AGIE is one of the MAGIC clients,both AGIE and
MAGIC willperform prioritization(AGIE at the message level and MAGIC at the
packet
level).To optimize how both work together,the AGIE connection profiles
need to be mapped to MAGIC's Profiles.In such an implementation,there is a pre-
determined mapping between AGIE connection path preferences and MAGIC Client
Profiles.AGIE's application layer lookup process is the same whether MAGIC is
present or not.However,when the message is sent to the destination broker's IP
address,port,and queue combination,MAGIC uses the requested MAGIC Client
Profile to make final path/route selection.

During configuration,the operator has mapped AGIE connection profiles and
MAGIC Client Profiles to unique parameters.Also,AGIE has registered as a MAGIC
client for each potential MAGIC Datalink profile to be used.Access control and
policies are then set and managed by the MAGIC implementation.

Again,AGIE uses a straight forward lookup to map destination AGIE server IDs and
connection profile names to a destination broker,IP address,port,and queue
combination along with the selected MAGIC connection profile.The message is
then sent broker-to-broker using that defined addressing scheme.The MAGIC
function routes the message appropriately using the best data-link available.
For MAGIC,any IP source and destination address and port may be processed.
While this example focuses on AGIE/Advanced Message Queuing Protocol
(AMQP),MAGIC will
and other protocols.

receive,validate,and process requests for Streaming,FTP,

The AGIE/AMQP path preference message is sent to MAGIC as a registration
request to MAGIC.MAGIC verifies whether the request
the MAGIC profile.MAGIC then answers the request as valid or invalid.This
process and the subsequent steps in communication are described in greater detail
in Section 4.1.2.1,Start and Normal Operation,and subsequent sections.

is valid when compared with

COMMENTARY

Addressing in MAGIC is dynamic;however,as long as there is an
AGIE/AMQP source address,the communication from the ground will
be completed and returned to the AGIE/AMQP source address as
long as the link is open.If the link is closed,there willnot be a return

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

ARINC SPECIFICATION 839-Page 25

path under the current specification of MAGIC.If a future

specification provides for a ground aware MAGIC,a return path could
be maintained.

AGIE Connection
Profile ID

Destination AGIE
Server ID

AMQP Endpoint
(Destination Broker IP,Port,Queue)

ToS

…

11(Frankfurt)
01(Small Safety)
02(Medium Safety)
11(Frankfurt)
03 (Large Best Effort) 11(Frankfurt)
15(Dubai)
01(Small Safety)
15(Dubai)
02(Medium Safety)
03(Large Best Effort) 15(Dubai)
20(Chicago)
01(Small Safety)
02(Medium Safety)
20(Chicago)
03(Large Best Effort) 20(Chicago)

Amqp://12.34.56.78:5672/exchange1/key1
Amqp://12.34.56.78:5673/queue
Amqp://12.34.56.78:5674/exchange2/key2
Amqp:/178.65.54.12:5672/exchange1/key1
Amqp://78.65.54.12:5673/queue
Amqp://78.65.54.12:5674/exchange2/key2
Amqp://32.10.01.23:5672/exchange1/key1
Amqp://32.10.01.23:5673/queue
Amqp://32.10.01.23:5674/exchange2/key2

38
19
19
31
15
15
31
15
15

Table 3-1: AGIE AMQP Mapping Table Example

The table above shows how AGIE would set-up its Connection Profiles on its
is the home-base of the airline,the Type
application level.Assuming that Frankfurt
of Service (ToS)field could be set to a higher value than for traffic intended for out
stations.This will differentiate messages that are directly going to the home and
those that first go to a server located in an out-station.

Client
Profile-ID

Flow-ID

Source
IP

Port

Destinatio

IP

n
Port

ToS
Range

Central Policy Profile
Profile-ID

…

AGIE

AGIE-Smal-Safety
172.29.1.1/32
AGIE-Medium-Safety
172.29.1.1/32
AGIE-Large-Best-Effort 172.29.1.1/32

0..65535 0.0.0.0/0
0..65535 0.0.0.0/0
0..65535 0.0.0.0/0

5672
5673
5674

16..47
0..31
0..31

Messaging-High
Messaging-Medium
Messaging-Low

Table 3-2:MAGIC Client Profile Example for AGIE

related(i.e.,in this case
The Client Profiles within MAGIC are capturing the client
AGIE)needs.Each different traffic type is represented by a Flow-ID.These flows
are defining boundaries in which the client is allowed to act.The above example
shows that only the onboard AGIE server with the IP Address 172.29.1.1 is allowed
to establish a connection from any onboard source port to any IP Address on
ground unless the destination port fits to one of the AGIE related flows and the ToS
field of the IP-Packets belonging to this flow are within the pre-defined range.

ARINC SPECIFICATION 839-Page 26

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

Central Policy Profiles
Profile-ID

wow

Data-Li
Order

nk-Module Sel
ToS-Range

ection Rules
DLM-Identifier

DLM-Link-Type

…

Messaging-High

Messaging-Medium

Messaging-Low

Ground

Air

Ground

Air

Ground

Air

1
2
3
4
1
1
2
3
1
1
2
1

16..47
24..47
32..47
38..47
16..47
0..31
8..31
16...31
0...1
0..31
16..31
0..31

Ethernet
WiFi

Gate-Link-Wired
Gate-Link-WiFi
Gate-Link-Cellular Data(2G/3G/4G)
Satcom-L
Satcom-L
Gate-Link-Wired
Gate-Link-WiFi
Gate-Link-Cellular Data(2G/3G/4G)
Satcom-L
Gate-Link-Wired
Gate-Link-WiFi
Satcom-L

Streaming-32k
Streaming-32k
Ethernet
WiFi

Streaming-8k
Ethernet
WiFi
Background

Table 3-3:MAGIC Core Profile Example for AGIE

The Central Policy Profile is capturing the main logic that allows MAGIC to choose
the correct links for the MAGIC non-aware clients and to check if a MAGIC-aware
client is allowed to request throughput on a dedicated link at a given time.The order
of the DLM-Identifier is giving MAGIC the capability to run through the list and to
select the first fitting available link.

The Link Profile belonging to the DLM is containing all the details that are needed to
communicate with the network infrastructure.An example would be to use EAP-TLS
as a link internal security measure to communicate with the home-base
infrastructure(e.g.,in a hangar).Another example would be the Access-Point-Name
(APN)of a Cellular data network (such as Long Term Evolution (LTE)).

3.5 Certification and Partitioning Considerations

MAGIC is a communication service that can offer its functionality to different
domains.This may impose certification and partitioning requirements to the system
design.As mentioned in previous sections,the MAGIC internal functions and the
MAGIC controlled external functions could be hosted in different domains.This
requires proper partitioning of the MAGIC internal functions.Some MAGIC internal
functions are considered as shared resource (e.g.,CM).Some MAGIC internal
functions are client specific and could reside in the same domain as the MAGIC
client

Interface Controller).

resides(e.g.,Client

To ensure that MAGIC fulfills required functional needs and can be certified,
segregation between the functions belonging to the domain where client resides and
the functions shared within MAGIC is required.This segregation may be achieved
by hosting some functions physically on a separated hardware,but it also can be
demonstrated by hosting these parts on a virtual environment on the same physical
unit.

The hosting environment of the functions must be secure,and the shared functions
must be protected against potential threats from the domains where clients reside.
An example of a threat would be the terminating functionalities (e.g.,the CIC)within
one domain.
The interface between the shared domain-independent functions and the domain-
dependent functions must be robust and secured from potential threats.

ARINC SPECIFICATION 839-Page 27

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

The shared area must be protected with additional mechanisms(e.g.,a firewallwith
a higher security assurance level).More detailed guidance is provided in
Appendix B.

3.6 Assumptions and Constraints

MAGIC is an IP-based system that is able to provide status information about
components belonging to it and about components directly managed by it.

The MAGIC standard is developed in accordance with the following assumptions:

· MAGIC is not able to manage the complete communication chain from end-

to-end,as there are several
transparent to the end users and also to MAGIC itself.

intermediate hops in the chain that are totally

· MAGIC is designed to manage only IP-based communications,excluding

ACARS and other non-IP link types.
· MAGIC may benefit from a management

interface to a ground system,on

certain links(e.g.,satellite links),to enable traffic prioritization in the ground-
to-aircraft direction.

COMMENTARY

It is assumed that the ground system is owned by the service
provider of that
standard.This standard only defines the interface to such systems on
ground.

link.The ground system definition is not part of this

3.6.1 Concept of Data Flow Operations

This section addresses the following DFOs:

·

·

·

·

Central MAGIC data transfer(DFO1)

Central MAGIC priority IP messaging(DFO2)

Federated MAGIC data transfer(DFO3)

Federated MAGIC priority IP messaging(DFO4)

3.6.1.1 Central MAGIC Data Transfer(DFO1)

The Airlines as stakeholder are flying with single onboard MAGIC services that each
are connected to multiple off-board communications links.

ARINC SPECIFICATION 839-Page 28

Stakeholder
Operation

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

Airline

Upload or download files to clients,including MAGIC-aware and non-
MAGIC-aware clients and access to any or all applicable communication
links.Example use cases for these operations are :

1.Electronic software distribution
2. Electronic Flight Bag ( EFB) data upload
3.Flight Operations Quality Assurance(FOQA)data download
4.Engine monitoring data download

Expectation The expectation is that all data transfers are efficient with MAGIC-aware
clients and highly adaptable;low cost,prioritized,effective per MAGIC
management principles,including transfer of large and small files during
one or more transactions through broken IP connectivity (air and
ground).In addition,the following assumptions apply:

·Airlines define applicable links.
·MAGIC-aware clients are tuned to dynamic link status.
·Files and small messages are efficiently transferred.
·MAGIC manages overall communication link efficiently.

3.6.1.2 Central MAGIC Priority IP Messaging(DFO2)

The Airlines as stakeholder are flying with single onboard MAGIC service with
multiple off-board links.

Stakeholder
Operation

Airline
Upload or download messages to client,including MAGIC-aware and
non-MAGIC-aware clients and access to any or all applicable
communication links with a potential interface/leverage via
Aircraft/Ground Information Exchange(AGIE)at the client level.
Example use cases are:
messaging

1.EFB
2.Logbook
3.Health

management

Expectation The expectation is that all transfers are efficient and support real-time
and high-priority transfers.In addition,the following assumptions apply:

·Airlines define applicable links.
·Messages are efficiently and quickly transferred.
·MAGIC manages overall communication link efficiently.

·Large,low priority files will not block high priority real-time

messages.

3.6.1.3 Federated MAGIC Data Transfer(DFO3)

The Airlines as stakeholder are flying multiple,federated onboard MAGIC services
with multiple off-board links(i.e.,an AIS MAGIC selects a broadband In-Flight
Entertainment(IFE)link for an EFB connected to the AIS domain).

ARINC SPECIFICATION 839-Page 29

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

Stakeholder
Operation

Airline
Upload or download files to client using communication link from other
MAGIC implementation,including MAGIC-aware and non-MAGIC-aware
clients access any or all applicable communication links on other MAGIC
services.Example use cases are:

1.Electronic software distribution
2. EFB data upload
3. FOQA
download
4.Engine monitoring data download

Expectation The expectation is that all transfers are efficient with MAGIC- aware

clients,highly adaptable,low cost,prioritized,effective per MAGIC
principles across all MAGIC implementations,including transfer of large
and small files during one or more transactions through broken IP
connectivity (air and ground).In addition,the following assumptions
apply:

· Airlines define applicable links using a common configuration
·MAGIC-aware clients get link status from adjacent MAGIC

.

implementations and from remote MAGIC implementations.

·Files are efficiently transferred.
· Each MAGIC entity manages the overallcommunication link

efficiently of its links.

·Remote MAGIC treats data in accordance with agreed design

and policy and meets MAGIC objectives.

·Multiple federated MAGIC services are functioning in a fully

coordinated manner in terms of making efficient and effective
link selections at the aggregate level.

3.6.1.4 Federated MAGIC Priority IP Messaging(DFO4)

The Airlines as stakeholder are flying multiple federated MAGIC-compliant
implementations with multiple off-board links on its aircraft(i.e.,an AIS MAGIC
selects a broadband In-Flight Entertainment(IFE)link for an EFB connected to the
AIS domain).

ARINC SPECIFICATION 839-Page 30

Stakeholder
Operation

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

Airline

Upload or download Messages to client using communication links from
other MAGIC services,including MAGIC-aware and non-MAGIC-aware
clients that want to access any or all applicable communication links of
different MAGIC implementations.Example use cases are:

messaging

1. EFB
2.Logbook
3 . Health management( May interface with or leverage AGIE)

Expectation The expectation is that all transfers support real-time and high priority
transfers across MAGIC services. In addition, the following assumptions
apply:

· Airlines define applicable links using common configuration.
·MAGIC-aware clients get link status from remote MAGIC via

local MAGIC.

·Each MAGIC service manages each communication link's

overall

efficiently.

·Remote MAGIC services treat data in accordance with agreed

policy and design and MAGIC objectives.

·Messages are efficiently and quickly transferred.
· Large files will not block communication priority real- time

messages.

· Multiple MAGIC services are communicating and coordinate

actions among and between themselves.

3.6.2 Concept of Management Flow Operations

The following Management Flow Operations(MFOs)are described further below:

·

·

·

·
·

·

Airline changes Internet Service Provider(ISP),changes business
parameters,logging,etc.( MFO1)

Client developer releases new MAGIC-aware client (MFO2)

Link developer/service provider develops MAGIC-aware link (MFO3)

Airline or aircraft manufacturer adds(non)MAGIC-aware clients(MFO4)
Airline or aircraft manufacturer adds (non)MAGIC-aware communication
links(MFO5)

Airline or aircraft manufacturer adds distributed MAGIC(MFO6)

3.6.2.1 Airline Business Parameters Change(MFO 1)

The Airlines as stakeholder are flying one or more MAGIC services on an aircraft
(with different configurations on multiple platforms in case of multiple MAGIC
services).

ARINC SPECIFICATION 839-Page 31

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

Stakeholder
Operation

Airline
Modification of business parameters including changes of data priorities,
accounting,billing,logging/reporting,permissions,and existing policies.

Expectation An Airline modifying the business parameters expects that:

· All changes and modification are handled by changing MAGIC

parameters and configurations in a loadable file

·Airlines make changes in script file or similar and runs through a

tool that builds loadable files

·Same or similar format across airline fleet of MAGIC

implementations

·Single entry system for multiple MAGIC implementations on any

single platform

·No code changes are required
·No re-integration or re-certification is required
·MAGIC accepts the inputs and manages accordingly
·Functions correctly even if some clients or communication links

are not MAGIC- aware

3.6.2.2 Develop New MAGIC-Aware Client (MFO2)

A client developer as stakeholder is coding a client compliant to the MAGIC
standard for inclusion on existing airline MAGIC service(s).

Stakeholder
Operation

Expectation

Client( system/application) developer
Develop a MAGIC-aware client.

The expectation is that a new MAGIC-aware client complies with the
following:

· The resulting code operates as expected under all MAGIC

services

·AIl MAGIC interfaces and functions are sufficiently similar such

that all MAGIC services can operate under one software
configuration item

·All links can be accessed whether in distributed MAGIC

implementation or non-MAGIC-aware links

· Works across different supplier and aircraft manufacturer

implementations

·Minimizes or eliminates integration efforts on multiple platforms
and types and across aircraft manufacturers and airlines

ARINC SPECIFICATION 839-Page 32

3.6.2.3 Develop New MAGIC-Aware Link(MFO3)

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

A link developer or a service provider as stakeholder builds a new MAGIC-aware
link using MAGIC specification for adding link to existing airline MAGIC service(s).

Stakeholder

Operation

Expectation

Link developer or service-provider
Develop MAGIC-aware link.
The expectation is that a new MAGIC-aware link complies with the
following:

·The interface between MAGIC and new communication link(link

controller)operates as expected on all MAGIC implementations
·AIl MAGIC interfaces and functions are similar enough that the

link operates similarly in all MAGIC environments

· Can serve allclients on platform whether in distributed MAGIC

services or non-MAGIC-aware links

·Unique link features such as QoS management,billing,etc.,are
implemented effectively by link-agnostic MAGIC functions

·Developer works to build a back-end to link-agnostic interface

and does not need to go further into MAGIC

3.6.2.4 Add New Client

to MAGIC Implementation(MFO4)

An airline or an aircraft manufacturer as stakeholder is adding a client to an existing
MAGIC implementation.

Stakeholder
Operation

Airline or aircraft manufacturer
Integrate a MAGIC-aware client or a non-MAGIC-aware client into an
operational MAGIC environment.

Expectation The expectation is that the new client is routed to the off-board links
using MAGIC configuration files ONLY and it functions without need for
code modification.In addition,the following apply:

·Client is loaded and physically installed,and uses Ethernet

outside of MAGIC

·Updates configuration MAGIC configuration files and templates
·No unique integration required
·All available links can be accessed per policy
·MAGIC-aware clients take advantage of MAGIC features without

additional coding changes or integration effort for improved
client and communications link efficiency

·Operates on airline and aircraft manufacturer fleet-wide and on

various airplane platforms

3.0 FUNCTIONAL REQUIREMENTS OF MAGIC

ARINC SPECIFICATION 839-Page 33

3.6.2.5 Add Communication Link to MAGIC Implementation(MFO5)

An airline or an aircraft manufacturer as stakeholder is adding a new communication
link to an existing MAGIC implementation.

Stakeholder
Operation

Airline or aircraft manufacturer
Integrate a new MAGIC-aware or non-MAGIC MAGIC-aware
communication
Expectation The expectations are that:

into a running MAGIC environment.

link

·The communication link is interfaced to the clients using MAGIC
configuration files and link-agnostic mapping without any further
need for code modification

·Communication link is physically installed and connected to the

Ethernet outside of the MAGIC implementation

· MAGIC configuration files and templates are updated
· Integration only required to map the link controller to the link-

agnostic interface

·All available clients can be served per policy
·Unique link features(e.g.,QoS,bandwidth management)are

effectively handled by pre- defined MAGIC functions/ interfaces
and mapped specifically via a link-agnostic interface to the link

3.6.2.6 Integrate New MAGIC Service with an Existing Environment(MFO6)

An airline or an aircraft manufacturer as stakeholder is adding a MAGIC service with
new communication links to an existing MAGIC environment(i.e.,connect a new
IFE MAGIC service to an existing aircraft manufacturers AIS MAGIC environment).

Stakeholder
Operation

Expectation

Airline or aircraft manufacturer
Integrate a new MAGIC service to an existing environment.
The expectations are that:

·Two MAGIC services function together,without

integration or code changes,via MAGIC parameter files.
Further,allclients can access all links(limited per policy
only).

·MAGIC implementation outside of MAGIC may be added.
·MAGIC parameters and templates(MAGIC-to-MAGIC)are

updated.

·Each MAGIC service shares link information and routes,and
data flows in accordance with agreed design and policy.

·Data flows are per MAGIC management principles.
·All accounting,logging,and administration information is passed
( between clients and links)as

between MAGIC implementations
if it were one MAGIC implementation.

ARINC SPECIFICATION 839-Page 34

4.0 MAGIC INTERFACE DEFINITIONS

4.0 MAGIC INTERFACE DEFINITIONS

MAGIC offers a client interface to query the system on the current operation and to
issue commands to access and manage bandwidth services.For a MAGIC
implementation supporting multiple domains,a consistent frame of reference is
required to allow the implementation to prioritize one traffic flow against another
when it comes to providing bandwidth services.

Interfaces definitions (based on Figure 3-1 numbering)included in phase 1 of

this

document are as follows:

·

·

·
·

Interface 1:Client

Interface Protocol (CIP)including Diameter commands

Interface 3:Network interface for queuing and shaping

Interface 4:Common link management
Interface 5:Network interface for routing and firewalling

interface

Interface 7:Aircraft

interface such as state information

Future supplements to this document may include the following interface definitions:

·
·

·

·

Extension of the MAGIC-to-MAGIC Interface
Airline Provider versus Airline Hosted ground peer function interface

Aircraft Ground Management

Interface.

Configuration and Administration exchange interface between MAGICs

· Wire-level protocol definition of the Common Link Interface

A general description of
Overview.

these interfaces is provided in Section 2.3,Functional

4.1 Client Interface

Interface uses the Diameter Base Protocol and defines a

The MAGIC Client
Diameter extension.Diameter is an Authentication,Authorization,and Accounting
(AAA)protocol
access,IP mobility,etc.).The general concept provides a base protocol stack that
defines support for user sessions and accounting that can be extended to provide
AAA services to access new technologies.

for applications that provide connectivity services(e.g., network

Using Diameter enables utilization of existing RFC-compliant Diameter servers
along with some modification to be able to facilitate the MAGIC Client
Diameter is extended for the needs of bandwidth negotiation activities.The
definition of new Diameter codes and message types enable MAGIC to co-exist
within a protocol
is wellformed,session-based,and robust.Utilization of a
standard protocol also provides benefits in the deployment against firewalls,firewall
rules,routers,and domain security models.

Interface.

that

The Diameter Base Protocol (DBP)is well defined by RFC 6733(Diameter Base
Protocol),which also defines the minimum requirements for an AAA protocol suite
and is referred to as [DBP-RFC].

4.1.1

Authentication and Authorization Model

Each MAGIC-aware client that requests throughput via the communication services
managed by MAGIC will connect via the Client
Interface Protocol(CIP).In order to
ensure that a client connecting is allowed to use MAGIC services,it needs to be
authenticated.This authentication is used to identify which particular functions it

is

ARINC SPECIFICATION 839-Page 35

4.0 MAGIC INTERFACE DEFINITIONS

authorized to use within MAGIC.Examples of the authentication models could be
the following:

1.Simple username/password combinations to authenticate a client or remote
MAGIC service.User name and password information is stored within the
MAGIC profiles.

2.Certificate based authentication as described in ARINC Report 842.

COMMENTARY

The security of the communication link that the client interface is
using should not be a factor in the operation of the interface.
[DBP-RFC],Section 13,recommends the use of either Transport
Layer Security (TLS)or IPsec mechanisms to protect Diameter traffic.

In MAGIC,the authentication and authorization model to open a link can be included
in the initial"MAGIC-Client-Authentication-Request"(MCAR)request
throughput.

for utilizing

4.1.2 MAGIC Client Interface Command Overview

MAGIC is defined to be its own Diameter client type to avoid conflicts with existing
Diameter clients (such as NASREQ).The application ID used by MAGIC is
1094202169(hex 0x41383339,which means “A839”in ASCll characters)and the
application name is "MAGIC Client

Interface Protocol(CIP)."

The specific Vendor ID under which the MAGIC Application is associated with IANA
is 13712.This ID is registered to ARINC Industry Activities and it may be called-out
in other ARINC Standards,e.g.,as Enterprise ID in the ArincSwift-MIB defined by
ARINC Characteristic 781.

COMMENTARY
The MAGIC Application ID is subject to be confirmed by the Internet
Assigned Numbers Authority(IANA)in case it needs to be a public
Internet Engineering Task Force(IETF)protocol number as agreed in
the Memorandum of Understanding between IETF and the Internet
Corporation for Assigned Names of Numbers(ICANN)concerning
IANA [RFC 2860].MAGIC has elected to use the vendor specific
implementation of [DBP-RFC]since less coordination work with IANA
is required for this type of ID.

Because authentication and authorization mechanisms vary among Diameter
applications,the Diameter Base Protocol does not define command codes and
Attribute-Value-Pairs(AVPs)specific to authentication and authorization.It
responsibility of the services to define their own messages and corresponding
attributes based on their own characteristics.

is the

As per [DBP-RFC]each command has assigned a command code.Each command
code comprises two parts.MAGIC uses the Request/Answer pair concept (as
introduced in [DBP-RFC])for messages that are initiated by client nodes only,but
also for messages that can be initiated by both nodes.For pairs initiated by the
MAGIC server nodes only,MAGIC uses a Report/Answer pair to exchange the
information.MAGIC only specifies new commands that willbe initiated by only one
node(client node or server node),but

it specifies no commands that can be initiated

ARINC SPECIFICATION 839-Page 36

4.0 MAGIC INTERFACE DEFINITIONS

by both nodes (such as the Capability-Exchange-Request (CER)of the Diameter
Base Protocol).

The message sub-type is identified via the 'R'bit in the Command Flags field of the
Diameter header.If this bit
is
not set,the message is an answer to a request or to a report.

is set,the message is a request or a report.If the bit

Every Diameter message must contain a command code in its header's Command-

Code

field.

The header's Command-Code is used to determine the action to be taken for a
particular message.MAGIC extends the Diameter Base Protocol with some new
functions and thus defines new commands and AVPs.The following table defines
the messages that are reused out of the Diameter Base Protocol ( Definition =DBP-
RFC)or are new defined within MAGIC(Definition =MAGIC):

Definition Code
257

DBP-RFC

258

274

275

282

280

MAGIC

100000

100001

100002

100003

100004

100005

100006

ID
CER
CEA
RAR
RAA
ASR
ASA
STR
STA
DPR
DPA

DWR
DWA
MCAR
MCAA
MCCR
MCCA
MNTR
MNTA
MSCR
MSCA
MSXR
MSXA
MADR
MADA
MACR
MACA

Diameter Message Name
Capabilities-Exchange-Request
Capabilities-Exchange-Answer
Re-Authentication-Request
Re-Authentication-Answer
Abort-Session-Request
Abort-Session-Answer
Session-Termination-Request
Session-Termination-Answer
Disconnect-Peer-Request
Disconnect-Peer-Answer
Device-Watchdog-Request
Device-Watchdog-Answer
MAGIC-Client-Authentication-Request
MAGIC-Client-Authentication-Answer
MAGIC-Communication-Change-Request
MAGIC-Communication-Change-Answer
MAGIC-Notification-Report
MAGIC-Notification-Answer
MAGIC-Status-Change-Report
MAGIC-Status-Change-Answer
MAGIC-Status-Request
MAGIC-Status-Answer
MAGIC-Accounting-Data-Request
MAGIC-Accounting-Data-Answer
MAGIC-Accounting-Control-Request
MAGIC-Accounting-Control-Answer

Initiator/Responder
Both MAGIC nodes
Both MAGIC nodes
MAGIC Server
MAGIC Client
MAGIC Server
MAGIC Client
MAGIC Client
MAGIC Server
Both MAGIC nodes
Both MAGIC nodes
Both MAGIC nodes
Both MAGIC nodes
MAGIC Client
MAGIC Server
MAGIC Client
MAGIC Server
MAGIC Server
MAGIC Client
MAGIC Server
MAGIC Client
MAGIC Client
MAGIC Server
MAGIC Client
MAGIC Server
MAGIC Client
MAGIC Server

command-pairs

The
DWR/DWA are unchanged with respect to the standard [DBP-RFC]definition.

CER/CEA,RAR/RAA,ASR/ASA,STR/STA,DPR/DPA,and

All new functions described in this document follow the recommendations in [DBP-
RFC]:

·

·

Modified "Augmented Backus-Naur Form(ABNF)"for Syntax Specifications
referred to as Command Code Format (CCF)as defined in [DBP-RFC],
Section 3.2.
Diameter Command Naming Conventions as

in [DBP-RFC],

defined

Section

3.3.

4.1.2.1 Start and Normal Operation

The sequence diagram below shows the exchange of Diameter messages between
a Client,MAGIC,and Link Controller at start-up time and during normal operation.
During operation a client can authenticate multiple times depending on the desired

ARINC SPECIFICATION 839-Page 37

4.0 MAGIC INTERFACE DEFINITIONS

needs.Ifa client may want to open a different type of service,it needs to
authenticate another time.This behavior is shown in the below diagram.

Chent

MAGIC

Link Controllar

tpbytef

0

to

(

鸟

nc

wnccw

heCA

kMCg

Deosdg

0m

DwglDw

oG0rko

cCc

Dpu0

De

o

Cz4

Dge

Dn

oc

wc

oc

iDM

CA

twCA

a

ce

oscon

a

Cs4

Dw

Dnk

gew

gu( TM

wocE

osw4

Se

uveST

WoG0ICC

T

04

Dpes0

De

0

Deo0

De

o

Dayg

o0

De WGCsenw dtoecrtwe

negta

cgb4te

sgnghsesso

mc

dg

MNE

azhertcafon

nchrirn

cgkatonateMGC

Wodhd2

购 当 种
N

9

nápaton

te

omsct加 s

Mhecrt
4.MAGGCeematoCharol

o tonmu

e grcund

CCF

dg

CC.

WGC

N

sando

threettecrt

e

ppeaU

地Ctoertt

del

arkcatos

ony00TTNna

clert w

Te
ehegc unodnetoLhreo engse
do
o

afte

8
MG
Co
L*Canart6te
ofhe
t
n

veso
tvsh
p
p008pois
meDteclem

4ACCRC
MWOKC

tas

n

sthe sas

imas

n 从 GC.

The

cetdoes

notresde

gownd

m

ond

临 essdw

n

ouros

v

8nd
odtohe

rThe
ve

and

ring

tetaontt

on

The cleraicaton w

caotegnd

0ed

ntsnaeM0086wGCDe
500

CCR

tatlkcor
MWAGCendsaespo

l
teneod
backqoicaton

P

aher

nt

.

Figure 4-1:Start and Normal Operation

The exchange is only terminated by errors of one entity as described in the following
section.

ARINC SPECIFICATION 839-Page 38

4.0 MAGIC INTERFACE DEFINITIONS

4.1.2.2 Additional

'Event-Driven'Operations

The previous section described a use-case where a client requested throughput for
each of its communication purposes under the assumption that the conditions will
not change until the client has finished the transmission and a request has been
made to close the link.The following sections identify additional triggers that may
cause changes to the throughput or closure of the link due to changed conditions
and availabilities.

4.1.2.2.1 Client Not Available/Restart

A client may no longer be available or restarted.In these cases,MAGIC recognizes
the absence by the Device-Watchdog-Request and closes all
links of these clients.
After restart or re-availability of the client,the client must authenticate again and
launch the start and normal operation process as described in Section 4.1.2.1.

4.1.2.2.2 Change of Session Parameters

If there is a change in the parameters during a session,such as MAGIC must
reduce the allotted throughput,MAGIC informs the client,which sessions are
affected and the changed values.

CLl

MAGIC

Link Controller

uanc0ov

C

Boru0

Do

r0

c

eMC

Davo

sprotwn

Duk

n

WOG
ac

MCCR

n
dNT4

MCCA

thuy0

or0

benw

Asgtheetwortstogoanuria
oegoundw
h
pro
akaon
A
tasedo1

Fsoso
WGCn
nadand

pes

poar

rdher

lbrad
ecangeo
qnt

ionte

reCLes

otteik

e%

Figure 4-2:Change Operations-Change of Session Parameters

4.1.2.2.3 Link Controller Failure

The sequence diagram below shows the Diameter messages which are exchanged
in case of a link controller failure (e.g.,complete connection loss).MAGIC sends a
notification message if the link properties are changed unexpectedly.

4.0 MAGIC INTERFACE DEFINITIONS

ARINC SPECIFICATION 839-Page 39

Cliont

MAGIC

Link Controllor

MAGIC Noficaon Reper MNTF
MAGIC NobficaticnAnswer(MNTA)

Device-Watchdog-equest (DWR)

Davce-WalchogAn8wr IDWA)

MAGIC detects modem failure(e.g.Link disappeared).

MAGIC informs application.Idenbification

is sent to each affected link.

MAGIC-Communicaton-Chango-Fequat(MCCR)

OpenLink()

MAGC-Comunealio Change-Anewer (MCCA)

The cient ties to ne-open the session.if the link
falure sill exist,the request is answered with an
erTor messB9e.
The dient has to request periodicaly for a new
s05Sion.

Figure 4-3:Change Operations-Link Controller Failure

4.1.3 MAGIC Diameter Command Definition

This section defines the extension to the Diameter Base Protocol.Each command
described in this section is comprised of two messages.One is the message that a
client is sending to the MAGIC server and the other is the one MAGIC is sending to
a client.Commands initiated by the client and answered by MAGIC are called
Request/Response pairs.Commands initiated by MAGIC and answered by a client
are called Report/Answer pairs.

4.1.3.1 Client Authentication Command Pair

The client authentication Request/Answer command pair is used to authenticate and
authorize a client with the MAGIC Server.The message and process does not
precisely follow the Diameter standard since in Diameter,there must be a client
authentication request to create a data session.
In MAGIC,it is possible to just authorize a client without immediately starting a data
session.An example is a client which only wants status reports.In that case,the
client authentication command pair should not specify any AVPs containing request
session specific parameters

like QoS,TFTs,links,profiles,etc.

To start a session later,the communication change command pair is used.
The sequence diagram in Section 4. 1 . 2 . 1 shows a typical client authentication

process.

4.1.3.1.1 Client Authentication Request

The MAGIC-Client-Authentication-Request(MCAR)message is used by the client
get authorized at the MAGIC server and to request services.Two types of services
are available to a client.A client can get authorization to get status information
about MAGIC and/or
contain allDiameter Base Protocol attributes as described in [DBP-RFC]for
authenticating a client.The format of a MCAR messages is defined as follows:

its connected communication links.A MCAR message must

to

ARINC SPECIFICATION 839-Page 40

4.0MAGIC INTERFACE DEFINITIONS

<MCAR>::=<Diameter Header:100000,REQ>

<Session-Id>
{Origin-Host}
{Origin-Realm}
{Destination-Realm}
{Auth-Application-ld}

optional [Session-Timeout]
optional [Client-Credentials]
optional [Auth-Session-State]
optional [Authorization-Lifetime]
optional [Auth-Grace-Period]
optional [Destination-Host]
optional [REQ-Status-Info]
optional [Communication-Request-Parameters]

★ [AVP]

Note that even clients that do not want
must also provide a Session-Id AVP in the client authentication process.This AVP is
used in subsequent messages to identify the client and its session.

to create any data session to the ground

MAGIC requires authentication of each client.If no certificate
is implemented,the Client-Credentials AVP-Group identifies a client by
name and password.Both values correspond to the user

based authentication

its user

identification in MAGIC's

Client

Profiles.

The REQ-Status-Info AVP may be added to the MCAR.It needs to be filled with the
appropriate value to get

the desired level of status information from MAGIC.

The Communication-Request-Parameters AVP-Group may
and filled with the requested profile or
communication to ground.If
this AVP is
shalltreat

the MCAR as a request

throughput values required to start
included in the MCAR,the MAGIC server

to negotiate bandwidth and establish a

be added to the MCAR

communication session via a suitable link controller.

A client can also put both AVPs(REQ-Status-Info AVP and Communication-
Request-Parameters AVP-Group)into the request

it needs status

in case

information,but also needs

to

start

the communication

immediately.

The detailed format of

the AVPs is described in Attachment

1.

4.1.3.1.2

Client Authentication Answer

The MAGIC-Client-Authentication-Answer(MCAA)message
by MAGIC
respond to a MCAR message.The MCAA contains all necessary information
needed to authenticate and authorize a client and to start
or optionally available).The format of a MCAA is shown below:

used

is

the service(as requested

to

4.0 MAGICINTERFACE DEFINITIONS

ARINC SPECIFICATION 839-Page 41

<MCAA>::=<Diameter Header:100000>
<Session-ld>
{Result-Code}
{Origin-Host}
{Origin-Realm}
{Auth-Application-Id}
{Server-Password}
{Auth-Session-State}
{Authorization-Lifetime}
{Session-Timeout
}
{Auth-Grace-Period}
{Destination-Host}

[MAGIC-Status-Code]

optional[ Failed- AVP]
optional
optional[Error-Message]
optional
optional

[REQ-Status-Info]
[ Communication- Answer- Parameters]

*[AVP]

include the REQ-Status-Info AVP in the answer if it was included in the

MAGIC shall
request,but the granted level of information may be different from the request
depending on the client's rights.

include the Communication-Answer-Parameters AVP-Group if the

included the Communication-Request-Parameters AVP-Group and

MAGIC shall
request
complete the answer with the values it received from the link that best fits the
request and the client's rights.
MAGIC shall
answer is coming back from the correct MAGIC Server.The client can decide
whether it wants to use the AVP for validation of the MAGIC Server or not.If it does
not validate the MAGIC server,it just can ignore this AVP.

include a Server-Password AVP to allow the client to verify that the

MAGIC shall reject the request,if a client is not allowed for the requested services.
The MAGIC-Client-Authentication-Answer may include the MAGIC-Status-Code

AVP and the Error-Message AVP in case something went wrong(e.g., the link could
not be opened,etc.).

4.1.3.2 Communication Change Command Pair

The communication change request/answer command pair is used for the initiation
of a communication link(if not already done within the client authentication
command)or for the modification of an existing link.

The Sequence Diagram in Section 4.1 .2 .1 shows also a typical communication
change process if a client requests establishment change or closure of a link.
A client can issue a MCAR command without creating a session and then issue a
MAGIC-Communication-Change-Request (MCCR)message to create a session or
issue a MCCR to modify an already existing session.

4.1.3.2.1 Communication Change Request

This message is very similar to the MCAR command but without the authentication
AVPs.The following AVPs are included in the MCCR command:

ARINC SPECIFICATION 839-Page 42

4.0 MAGIC INTERFACE DEFINITIONS

<MCCR>::=<Diameter

Header:100001,REQ>

<Session-ld>
{Origin-Host}
{Origin-Realm}
{Destination-Realm}
{Communication-Request-Parameters}

*[AVP]

4.1.3.2.2 Communication Change Answer

This message is provided by the MAGIC server in response to a MCCR.This
command is very similar to the MCAA command but without the authentication

AVPs.The following AVPs make up the MAGIC-Communication-Change-Answer
(MCCA)message:

<MCCA>::=<Diameter

Header:100001>

<Session-ld>
{Result-Code}
{Origin-Host}
{Origin-Realm}

optional[Failed-AVP]
optional[MAGIC-Status-Code]
optional

[Error-Message]

{Communication-Answer-Parameters}

*[AVP]

The MAGIC-Communication-Change-Answer message may include the MAGIC-
Status- Code AVP and the Error- Message AVP in case something went wrong(e. g. ,
the link could not be modified,etc.).

4.1.3.3 Notification Message Command Pair

The Notification report/answer command pair is used as a means to notify the client
that there is a change in the negotiated parameters as agreed through a
MCAR/MCAA or a MCCR/MCCA interaction.The parameters returned in the
MAGIC-Notification-Report(MNTR)command are the similar to those returned in a
MCCA command.All values are returned on the session not just the parameters
that were modified.

4.1.3.3.1 Notification Report

The following AVPs make up the MAGIC-Notification-Report

(MNTR)message:

<MNTR>::=<Diameter Header:100002,REQ>

<Session-ld>
{Origin-Host}
{Origin-Realm}
{Destination-Realm}
{ Communication- Report- Parameters}

optional
optional

[MAGIC-Status-Code]
[Error-Message]

*[AVP]

ARINC SPECIFICATION 839-Page 43

4.0 MAGIC INTERFACE DEFINITIONS

A Notification report may include the MAGIC-Status-Code AVP and the Error-
Message AVP in case something went wrong(e.g.,the link could not be opened,
etc.) .

4.1.3.3.2 Notification Answer

A MAGIC-Notification-Answer(MNTA)message
by the client

in response to the reception of a MNTR message.

is an

acknowledgement produced

The following AVPs make up the MAGIC-Notification-Answer(MNTA)message:

<MNTA>::=<Diameter Header:100002>
<Session-ld>
{Result-Code}
{Origin-Host}
{Origin-Realm}

optional[Failed-AVP]
*[AVP]

A client may include the Failed-AVP AVP in case it did not understand the message
by indicating which AVP could not be understood.

If a client
event without changing the previously provided information.

is sending back a Failed-AVP AVP the Diameter Server should log this

4.1.3.4 Status Change Message Command Pair

The status change report/answer command pair
inform the client about
subscribing to them during the MCAR request using the REQ-Status-Info AVP.If
client uses this AVP to request a:

is initiated by the MAGIC server to
link status changes.These reports are delivered to clients by
the

·

·

·

General MAGIC Status,the Registered-Clients AVP is included in the
answer to authorized clients.
General DLM status,the DLM-List AVP-Group is included in the answer to
authorized clients.The group contains general
the client
DLM info.

information for each DLM that
is not contained in the

is allowed to use.The detailed link status list

Detailed DLM status,the DLM-List AVP-Group is included in the answer to
authorized clients.The group contains general
the client
the DLM info in case the requesting client has the appropriate rights.

is allowed to use.The detailed link status list

information for each DLM that

is also contained in

The client can also request
and Detailed DLM Status.The possible combinations are described in

combinations of MAGIC Status,General DLM Status,

Attachment

1.

4.1.3.4.1 Status Change Report

The following AVPs make up the MAGIC-Status-Change-Report(MSCR)message:

ARINC SPECIFICATION 839-Page 44

4.0MAGIC INTERFACE DEFINITIONS

<MSCR>::=<Diameter Header:100003,REQ>

<Session-Id>
{Origin-Host}
{Origin-Realm}
{Destination-Realm}

optional [MAGIC-Status-Code]
optional
optional
optional [Registered-Clients]
[DLM-List]
optional

[Error-Message]
[Status-Type]

*[AVP]

A status change report may include the MAGIC-Status-Code AVP and the Error-
Message AVP in case something went wrong(e.g.,the
etc.).

link dropped

unexpectedly,

4.1.3.4.2

Status Change Answer

A MAGIC-Status-Change-Answer(MSCA)message
a MAGIC-Status-Change-Report
response

to

provided

is
(MSCR).

by a

client

in

The

following AVPs make

up

the

MAGIC-Status-Change-Answer(MSCA)message:

<MSCA>::=<Diameter Header:100003>

<Session-Id>
{Result-Code}
{Origin-Host}
{Origin-Realm}
[Failed-AVP]

optional

*[AVP]

A client may include the Failed-AVP AVP in case it did not understand the message
by indicating which AVP could not be understood.

If a client
event without changing the

previously provided information.

is sending back a Failed-AVP AVP the Diameter Server should log this

4.1.3.5 Status Message Command Pair

The Status
request/answer
is a onetime only request
(unless
already done so through a MCAR or MCCR command)to future Status Change
Reports.

information.It does not subscribe the client

command pair
for

is used to exchange status information.It

4.1.3.5.1

Status

Request

A MAGIC-Status-Request(MSXR)message

is issued

by a client

to

request

information about
returned is the same as

the state of

the MAGIC server and/or

the links.The information

in the MAGIC-Status-Change-Report message.

The following AVPs make

up the MAGIC-Status-Request

(MSXR)message:

ARINC SPECIFICATION 839-Page 45

4.0 MAGIC INTERFACE DEFINITIONS

<MSXR>::=<Diameter Header:100004,REQ>

<Session-ld>
{Origin-Host}
{Origin-Realm}
{Destination-Realm}
{Status-Type}

*[AVP]

The Status-Type AVP to request the desired information can be set to request:

● General MAGIC Status
· General DLM Status
· Detailed DLM Status

The client can also request combinations of MAGIC Status,General DLM Status,
and Detailed DLM Status.The possible combinations are described in
Attachment 1.The format of this AVP is the same as the format of the REQ-Status-
Info AVP.

4.1.3.5.2 Status Answer

A MAGIC-Status-Answer(MSXA)message is issued by a MAGIC server in
response to a MAGIC-Status-Request.The Status-Type AVP may be changed by

MAGIC to another value based on the authorization level of the client.The status
request may also be rejected in case the client does not have the appropriate rights.

If the client uses the Status-Type AVP to request a:

· General MAGIC Status,the Registered-Clients AVP is included in the

·

·

answer to authorized clients.
General DLM status,the DLM-List AVP-Group is included in the answer to
information for each DLM that
authorized clients.The group contains general
the client is allowed to use.The detailed link status list is not contained in the
DLM info.
Detailed DLM status,the DLM-List AVP-Group is included in the answer to
authorized clients.The group contains general
the client is allowed to use.The detailed link status list is also contained in
the DLM info in case the requesting client has the appropriate rights.

information for each DLM that

The client can also request combinations of MAGIC Status,General DLM Status,
and Detailed DLM Status.The possible combinations are described in

Attachment 1.

The following AVPs make up the MAGIC-Status-Answer(MSXA)command:

ARINC SPECIFICATION 839-Page 46

4.0MAGIC INTERFACE DEFINITIONS

<MSXA>::=<Diameter Header:100004>

<Session-ld>
{Result-Code}
{Origin-Host}
{Origin-Realm}
{Status-Type}
[MAGIC-Status-Code]
[Error-Message]
[Failed-AVP]
[Registered-Clients]
[DLM-List]

optional
optional
optional
optional
optional

*[AVP]

A status answer may include the MAGIC-Status-Code AVP and the Error-Message
AVP in case something went wrong(e.g.,the client did not have the appropriate
rights to get the requested information,etc.).

4.1.3.6 Accounting Data Message Command Pair

MAGIC shall record session information for the purposes of accounting and cost to
service correlation by using Call Data Records(CDR).

The MAGIC server initiates the recording of CDRs for the entire negotiated session.
The accounting data request/answer command pair allows a client to request a list
of identifiers for existing CDRs or the actual CDR if a CDR identifier is specified.The
entire list of CDRs can be received,or the CDRs,for specified users or specified
sessions,can be received.

In order to retrieve the data of a CDR,a request to retrieve the CDR identifier must

be made.

4.1.3.6.1 Accounting Data Request

The following AVPs make up the MAGIC-Accounting-Data-Request (MADR)

message:

<MADR>::=<Diameter Header:100005,REQ>

<Session-ld<
{Origin-Host}
{Origin-Realm}
{Destination-Realm}
{CDR-Type}
{CDR-Level}
[CDR-Request-Identifier]

optional

*[AVP]

The MAGIC-Accounting-Data-Request(MADR)message is accomplished by setting
the CDR-Type AVP to LIST_REQUEST.Once a CDR identifier is retrieved,an
MADR request with a CDR-Type AVP set to DATA_REQUEST could be made with
the CDR-Request-Identifier AVP set to an identifier retrieved from the first request.
The CDR-Level AVP is defining what level of information is requested.It can be set
to the following level:

·

·

·

All accounting data

User dependent accounting data

Session dependent accounting data

ARINC SPECIFICATION 839-Page 47

4.0 MAGIC INTERFACE DEFINITIONS

·

Accounting identifier dependent accounting data

In case all accounting data has been chosen as
AVPs is not added in the request.

In case the user dependent accounting data level
provided in the CDR-Request-Identifier AVP.

level,the CDR-Request-Identifier

is chosen,the user-name must be

In case the session dependent accounting data level
be provided in the CDR-Request-Identifier AVP.

is chosen,the session ID must

In case the accounting identifier dependent accounting data level
accounting identifier must be provided in the CDR-Request-Identifier AVP.

is chosen,the

4.1.3.6.2 Accounting Data Answer

The MAGIC-Accounting-Data-Answer(MADA)message is produced in response to
the MADR.Depending on the request,this message may contain either a list of

CDR identifiers or actual CDR records.

The following AVPs make up the MAGIC-Accounting-Data-Answer(MADA)

command:

<MADA>::=<Diameter Header:100005>

<Session-ld>
{Result-Code}
{Origin-Host}
{Origin-Realm}
{CDR-Type}
{CDR-Level}

optional [CDR-Request-Identifier]
optional [CDRs-Active]
optional [CDRs-Finished]
optional [CDRs-Forwarded]
optional [CDRs-Unknown]
optional [MAGIC-Status-Code]
optional [Error-Message]
optional[Failed-AVP]
*[AVP]

the CDRs-Active,the CDRs-Finished,CDRs-Forwarded,and/or

The answer is echoing back the values that have been set
adds
Unknown AVP-Groups in dependence of
and fills them with the requested type of

the availability of such accounting records
information.

in the request.MAGIC
the CDRs-

A accounting data answer may include the MAGIC-Status-Code AVP and the Error-
Message AVP in case something went wrong(e.g.,the client did not have the
appropriate rights to get

the requested information,etc.).

4.1.3.7 Accounting Control Message Command Pair

When starting a session,an Accounting Record (also known as a CDR)is created
and populated with information about
the session terminates.The
CDRs can be read by clients using the accounting data request/answer command
pair.Usually,the Accounting Records are generated and stopped automatically
(based on the session messages and control requests).

the session until

ARINC SPECIFICATION 839-Page 48

4.0MAGIC INTERFACE DEFINITIONS

4.1.3.7.1 Accounting Control Request

The MAGIC-Accounting-Control-Request
CDRs for individual sessions.Restart means that
is possible to stop an accounting
record and immediately start a new one for the specified session.Clients that do not
have authority to make accounting control requests would receive an error
MAGIC-Accounting-Control-Answer(MACA).

(MACR)message can be used to restart

in the

it

The following AVPs make up the MAGIC-Accounting-Control-Request

(MACR)

message:

<MACR>::=<Diameter Header:100006,REQ>

<Session-ld>
{Origin-Host}
{Origin-Realm}
{CDR-Restart-Session-Id}

*[AVP]

The CDR-Restart-Session-Id AVP specifies the session that needs to be restarted.

4.1.3.7.2 Accounting Control Answer

The MAGIC-Accounting-Control-Answer(MACA)message
to the MACR.If a successful restart or start of a new CDR was performed,the
is returned.This value could in turn be used by the
accounting record identifier
to retrieve information specific to that CDR.
MAGIC-Accounting-Data-Request

is produced

in

response

The following AVPs make up the MACA command:

<MACA>::=<Diameter Header:100006>

<Session-ld>
{Result-Code}
{Origin-Host}
{Origin-Realm}
{CDR-Restart-Session-Id}

optional [MAGIC-Status-Code]
optional [Error-Message]
optional [Failed-AVP]
optional [CDRs-Updated]

*[AVP]

The MACA message is containing the CDRs-Updated AVP-Group in case the
request could be performed.The AVP-Group contains a list of CDR-pairs (old
stopped CRD and new started CDR)belonging to the request.

An accounting data answer may include the MAGIC-Status-Code AVP and the
Error-Message AVP in case something went wrong(e.g., the specified session did
not

exist,etc.).

4.1.4 Attribute Value

Pairs(AVPs)

Each Diameter command is based on multiple attribute value pairs(AVPs)that are

used to transfer dedicated information between the server and the client node.

This document
is reusing the Diameter Base Protocol AVPs as defined in [DBP-
RFC]Section 4.5 as a basis and is extending those AVPs with additional MAGIC
related AVPs.Please note that AVPs that have been defined in the [DBP-RFC]are

4.0 MAGIC INTERFACE DEFINITIONS

only briefly described here.For more detail about the unchanged [DBP-RFC]AVPs,
refer to the [DBP-RFC].

ARINC SPECIFICATION 839-Page 49

4.1.4.1 Diameter Base Protocol AVPs

This section provides a brief overview of all AVPs that are reused from [DBP-RFC].
The following sections provide further details when an AVP is treated differently than
originally intended in the Diameter Base Protocol.The following table describes all
AVPs from the Diameter Base Protocol and defines the way they are used within the
MAGIC environment.An AVP is used as defined in [DBP-RFC]if the column
"Usage"is empty,but is used in a different manner if that column refers to the
attachment within this specification.

MAGIC
Specifics

AVP-Name

Accounting-Realtime-Required

Accounting-Record-Number

Accounting-Record-Type

Accounting-Session-Id

Accounting-Sub-Session-Id

Acct-Application-Id

Acct-Interim-Interval

Acct-Multi-Session-Id

Auth-Application-Id
Auth-Grace-Period
Authorization-Lifetime
Auth-Request-Type
Auth-Session-State
Class
Destination-Host
Destination-Realm

Disconnect-Cause

Error-Message
Error-Reporting-Host
Event-Timestamp
Experimental-Result
Experimental-Result-Code
Failed-AVP

Firmware-Revision

Host-IP-Address
Inband-Security-Id
Multi-Round-Time-Out
Origin-Host

Code

Type

483

485

Enumerated

Unsigned32

480

Enumerated

44

287

259

85

50

258
276
291
274
277
25
293
283

273

281
294
55
297
298
279

267

257
299
272
264

OctetString

Unsigned64

Unsigned32
Unsigned32

UTF8String

Unsigned32
Unsigned32
Unsigned32
Enumerated
Enumerated
OctetString
DiamIdent
DiamIdent

Enumerated

UTF8String
DiamIdent
Time
Grouped
Unsigned32
Grouped
Unsigned32

Address
Unsigned32
Unsigned32
Address

First Definition
[DBP-RFC]
§9.8.7
[DBP-RFC]
§9.8.3
[DBP-RFC]
§9.8.1
[DBP-RFC]
§9.8.4
[DBP-RFC]
§9.8.6
[DBP-RFC] §6.9
[DBP-RFC]
§9.8.2
[DBP-RFC]
§9.8.5
[DBP-RFC] §6.8
[DBP-RFC] §8.10
[DBP-RFC] §8.9
[DBP-RFC] §8.7
[DBP-RFC] §8.11
[DBP-RFC] §8.20
[DBP-RFC] §6.5
[DBP-RFC] §6.6
[DBP-RFC]
§5.4.3
[DBP-RFC] §7.3
[DBP-RFC] §7.4
[DBP-RFC] §8.21
[DBP-RFC]S7.6
[DBP-RFC]s7.7
[DBP-RFC] §7.5
[DBP-RFC]
§5.3.4
[DBP-RFC]
§5.3.5
[DBP-RFC] §6.10
[DBP-RFC] §8.19
[DBP-RFC] §6.3

ARINC SPECIFICATION 839-Page 50

4.0 MAGIC INTERFACE DEFINITIONS

AVP-Name
Origin-Realm
Origin-State-Id

Product-Name

Proxy-Host

Proxy-Info

Proxy-State

Re-Auth-Request-Type
Redirect-Host
Redirect-Host-Usage
Redirect-Max-Cache-Time
Result-Code

Route-Record
Session-Binding
Session-Id
Session-Server-Failover
Session-Timeout
Supported-Vendor-Id

Termination-Cause
User-Name

Vendor-Id

Vendor-Specific-Application-
Id

4.1.4.2 MAGIC AVPs

Code
296
278

269

280

284

33
285
292
261
262
268

282
270
263
271
27

265
295
1

266

260

Type
DiamIdent
Unsigned32
UTF8String

DiamIdent

Grouped

OctetString

Enumerated
DiamURI
Enumerated
Unsigned32
Unsigned32

DiamIdent

Unsigned32
UTF8String
Enumerated
Unsigned32
Unsigned32

Enumerated
UTF8String
Unsigned32

MAGIC
Specifics

First Definition
[DBP-RFC] §6.4
[DBP-RFC] §8.16
[DBP-RFC]
§5.3.7
[DBP-RFC]
§6.7.3
[DBP-RFC]
§6.7.2
[DBP-RFC]
§6.7.4
[DBP-RFC] §8.12
[DBP-RFC] §6.12
[DBP-RFC] §6.13
[DBP-RFC] §6.14
[DBP-RFC] §7.1
[DBP-RFC]
§6.7.1
[DBP-RFC] §8.17
[DBP-RFC]S8.8
[DBP-RFC] §8.18
[DBP-RFC] §8.13 ATTACHMENT 1
[DBP-RFC]
§5.3.6
[DBP-RFC] §8.15
[DBP-RFC] §8.14
[DBP-RFC]
§5.3.3

ATTACHMENT 1

Grouped

[DBP-RFC] §6.11

This section provides a brief overview of all AVPs that are introduced by MAGIC.
The following sections provide further details about the usage of each new defined
AVP.The following table describes all AVPs in the MAGIC environment.

AVP-Name

Accounting-Enabled
Airport
Altitude
Auto-Detect
CDR-Content
CDR-Id
CDR- Info
CDR-Level
CDR-Request-Identifier
CDR- Restart- Session- Id
CDRs-Active
CDRs-Finished
CDRs-Forwarded
CDR-Started

Code

10036
10035
10034
10038
10047
10046
20017
10043
10044
10048
20012
20013
20014
10050

Type

Unsigned32
UTF8String
UTF8String
Enumerated
UTF8String
Unsigned32
Grouped
Enumerated
UTF8String
UTF8String
Grouped
Grouped
Grouped
Unsigned32

First
Definition
Attachment 1
Attachment 1
Attachment 1
Attachment 1
Attachment 1
Attachment 1
Attachment 1
Attachment 1
Attachment 1
Attachment 1
Attachment 1
Attachment 1
Attachment 1
Attachment 1

MAGIC
Specifics
§1.1.1.6.5.1
§1.1.1.6.4.3
§1.1.1.6.4.2
§1.1.1.6.5.3
§1.1.1.8.1.5
§1.1.1.8.1.4
§1.1.2.4.6
§1.1.1.8.1.2
§1.1.1.8.1.3
§1.1.1.8.2.1
§1.1.2.4.1
§1.1.2.4.2
§1.1.2.4.3
§1.1.1.8.2.3

4.0 MAGIC INTERFACE DEFINITIONS

ARINC SPECIFICATION 839-Page 51

AVP-Name

CDR- Start- Stop- Pair
CDR-Stoped
CDRs-Unknown
CDRs-Updated
CDR- Type
Client-Credentials
Client-Password
Communication-Answer-Parameters
Communication-Report-Parameters
Communication-Request-
Parameters
DLM-Allocated-Bandwidth
DLM-Allocated-Links
DLM-Allocated-Return-Bandwidth
DLM-Availability-List
DLM-Available
DLM-Info
DLM- Link- Status- List
DLM-List
DLM-Max-Bandwidth
DLM- Max- Links
DLM-Max-Return-Bandwidth
DLM-Name
DLM- QoS- Level- List
Flight-Phase
Gateway-IPAddress
Granted-Bandwidth
Granted-Return-Bandwidth
Keep-Request
Link-Alloc-Bandwidth
Link-Alloc-Return-Bandwidth
Link-Available
Link-Connection-Status
Link-Error-String
Link-Login-Status
Link-Max-Bandwidth
Link-Max-Return-Bandwidth
Link-Name
Link-Number
Link-Status-Group
MAGIC-Status-Code
NAPT-List
NAPT-Rule
Priority-Class
Priority-Type
Profile-Name
QoS-Level
Registered-Clients
REQ- Status- Info

Code

20018
10049
20015
20016
10042
20019
10001
20002
20003

20001

10007
10011
10009
10028
10005
20008
20010
20007
10006
10010
10008
10004
20009
10033
10029
10051
10052
10037
10018
10019
10013
10014
10020
10015
10016
10017
10054
10012
20011
10053
20006
10032
10025
10026
10040
10027
10041
10002

Type

Grouped
Unsigned32
Grouped
Grouped
Enumerated
Grouped
UTF8String
Grouped
Grouped
Grouped

Float32
Unsigned32
Float32
UTF8String
Enumerated
Grouped
Grouped
Grouped
Float32
Unsigned32
Float32
UTF8String
Grouped
UTF8String
UTF8String
Float32
Float32
Enumerated
Float32
Float32
Enumerated
Enumerated
UTF8String
Enumerated
Float32
Float32
UTF8String
Unsigned32
Grouped
Unsigned32
Grouped
UTF8String
UTF8String
Enumerated
UTF8String
Enumerated
UTF8String
Unsigned32

MAGIC
Specifics

First
Definition
Attachment 1 §1.1.2.4.7
Attachment 1 §1.1.1.8.2.2
Attachment 1 §1.1.2.4.4
§1.1.2.4.5
Attachment 1
Attachment 1 §1.1.1.8.1.1
Attachment 1 §1.1.2.5.1
§1.1.1.2.1
Attachment 1
§1.1.2.1.2
Attachment 1
§1.1.2.1.3
Attachment 1

Attachment 1

§1.1.2.1.1

§1.1.1.4.4
Attachment 1
Attachment 1 §1.1.1.4.8
Attachment 1 §1.1.1.4.6
Attachment 1 §1.1.1.6.3.1
Attachment 1 §1.1.1.4.2
Attachment 1 §1.1.2.3.2
Attachment 1 §1.1.2.3.4
Attachment 1 §1.1.2.3.1
Attachment 1 §1.1.1.4.3
Attachment 1 §1.1.1.4.7
Attachment 1 §1.1.1.4.5
Attachment 1 §1.1.1.4.1
Attachment 1 §1.1.2.3.3
Attachment 1 §1.1.1.6.4.1
Attachment 1 §1.1.1.6.3.2
Attachment 1 §1.1.1.6.1.5
Attachment 1 §1.1.1.6.1.6
Attachment 1 §1.1.1.6.5.2
Attachment 1 §1.1.1.5.8
Attachment 1 §1.1.1.5.9
Attachment 1 §1.1.1.5.3
Attachment 1 §1.1.1.5.4
Attachment 1 §1.1.1.5.10
Attachment 1 §1.1.1.5.5
Attachment 1 §1.1.1.5.6
Attachment 1 §1.1.1.5.7
Attachment 1 §1.1.1.5.2
Attachment 1 §1.1.1.5.1
Attachment 1 §1.1.2.3.5
Attachment 1 §1.1.1.1.9.2
Attachment 1 §1.1.2.2.3
Attachment 1 §1.1.1.6.3.3.3
Attachment 1 §1.1.1.6.2.1
Attachment 1 §1.1.1.6.2.2
Attachment 1 §1.1.1.7.1
Attachment 1 §1.1.1.6.2.3
Attachment 1 §1.1.1.7.2
Attachment 1 §1.1.1.3.1

ARINC SPECIFICATION 839-Page 52

4.0 MAGICINTERFACE DEFINITIONS

AVP-Name

Requested-Bandwidth
Requested-Return-Bandwidth
Required-Bandwidth
Required-Return-Bandwidth
Server-Password
Status-Type
TFTtoAircraft-List
TFTtoAircraft-Rule
TFTtoGround-List
TFTtoGround-Rule
Timeout

4.2 Link Management Interface

Code

10021
10022
10023
10024
10045
10003
20005
10031
20004
10030
10039

Type

Float32
Float32
Float32
Float32
UTF8String
Unsigned32
Grouped
UTF8String
Grouped
UTF8String
Unsigned32

MAGIC
Specifics

First
Definition
Attachment 1 §1.1.1.6.1.1
Attachment 1 §1.1.1.6.1.2
Attachment 1 §1.1.1.6.1.3
Attachment 1 §1.1.1.6.1.4
Attachment 1 §1.1.1.2.2
Attachment 1 §1.1.1.3.2
Attachment 1 §1.1.2.2.2
Attachment 1 § 1.1.1.6.3.3.2
Attachment 1 §1.1.2.2.1
Attachment 1 § 1.1.1.6.3.3.1
Attachment 1 §1.1.1.6.5.4

MAGIC is intended to support multiple simultaneous communication system

interfaces on an aircraft.MAGIC establishes a common interface mechanism to
independently developed communication link interfaces.MAGIC-aware
communication link interfaces by definition meet the common interface.Non-
MAGIC-aware communication link interfaces are mapped to the common interface
using the DLM.This approach allows a common interface to perform basic actions
for command and monitoring.

4.2.1 Overview

The Common Link Interface allows the Central Management (CM)to send
commands to the communication links.In the reverse direction,this interface allows
the CM to receive events from the links.Commands are used to control a link,
whereas events are used to monitor a link by providing status information.

The definition of this interface is based on the link layer interface,MIH_LINK_SAP,
as specified in IEEE 802.21-2008.In IEEE 802.21,MIH_LINK_SAP is the link layer
Service Access Point(SAP)that provides the interface to control and monitor
technology-specific links,also known as media-specific links.This abstract
is applied to each media-specific link system.When applied,the MIH_LINK_SAP
assumes the names and service primitives of the media-specific lower layer SAPs.It
is a mapping between the abstract MIH_LINK_SAP service primitives and the
media-specific lower-layer SAP service primitives.IEEE 802.21 does not specify
underlying transport services or an underlying wire-level
MIH_LINK_SAP interface.It
implementation specific matter.

is assumed the underlying interface is a local,

interface for the

interface

MAGIC reuses a subset of the MIH_LINK_SAP service primitives in the common
link management interface and adds a service primitive not defined by IEEE 802.21.

COMMENTARY

Although IEEE 802.21 does not specify an underlying wire-level
interface,MAGIC specifies the underlying wire-level
common link management interface may be defined in a future
Supplement to this standard.

interface for the

A MAGIC-aware link directly supports the common communication link interface
without any mapping between the interfaces.A non-MAGIC-aware link does not
directly support the common link management

interface and requires mapping

4.0 MAGICINTERFACE DEFINITIONS

between the interfaces.A MAGIC-aware link directly supports the wire-level

interface but a non-MAGIC-aware does not.

ARINC SPECIFICATION 839-Page 53

To support a non-MAGIC-aware link,a DLM is needed to map the common link
management
the common wire-level

interface with the technology-specific link interface.The DLM adapts

interface to the link-specific interface.

COMMENTARY
The definition of the DLM is outside the scope of MAGIC.It is defined
for each technology.

link management interface architecture is shown in Figure 4-4: MAGIC

The overall
Link Management
correspond to the like blocks and numbering in Figure 2-3.

Interface Architecture.The blocks and numbering in the figure

MAGIC Management

CMF
MH User

MH_SAP

Link

Management

MHF

Common Link Interface
MIH_LINK_SAP

2

Comm Link-
MAGIC-Aware

Common Unk Intertace
MHLINK_SAP

Data Link
Module

Media-specihcLink Interface

⑤

Comm Link-
Non-MAGIC-
Aware

Figure 4-4:MAGIC Link Management

Interface Architecture

COMMENTARY

The Link Management block provides the MIH_SAP for use by the
CM and a Common Link Interface(MIH_LINK_SAP)for the different
communication links.
The core of the link management is the Media-Independent Handover
Function (MIHF)from the IEEE 802.21-2008 standard.The link

management contains an MIHF instance for each link that is present
in the system.Refer to Figure 4-5.The MIH_SAP is a media
independent
Handover(MIH)user and an MIHF.A dedicated MIH Identifier(ID)is
used by the CM to address the different MIHF instances,thereby
allowing communication with a specific link.

interface between the CM as an Media Independent

ARINC SPECIFICATION 839-Page 54

4.0 MAGICINTERFACE DEFINITIONS

The MIHF Instance then maps the MIH ID to a link identifier-this
usually is a 1:1 mapping,although a 1:n mapping(several
identifiers)may also be possible.

link

CMF
MIH User

MH_ SAP

Link

MIHF
Instance

MIHF
Instance

Management

MH LINK_ SAP

MH LINK_ SAP

MH_LINK_SAP

Data Link
Module

MH LINK_ SAP

Link 1

Link 2

Figure 4-5: Example Architecture Illustrating MIHF Instances

COMMENTARY

Interface Handler(see Section 4.1).In the case

DLM messages can be triggered by requests coming from MAGIC
Clients,via the Client
of non-MAGIC-aware clients,the CM should exchange DLM
messages on behalf of those clients.The Common Link Interface
includes primitives that provide functionality that is similar to the client
interface messages for mapping client requests to the underlying
communication links.This has been considered when defining the
Common Link Interface primitives.

For comparison,the IEEE 802.21-2008 reference model
MIH_NET_SAP is an abstract interface of MIHF to provide transport services over
the data plane to support exchange of MIH information and messages between
remote MIHFs(In this case,the MIHFs are located in separate network nodes and
are not the MIHF instances represented in Figure 4-5).Presently,as mentioned
previously,MAGIC only uses a subset of the MIH_LINK_SAP primitives and adds
new primitives as permitted by IEEE 802.21.MAGIC may use the MIH_SAP for the
interface between the CM and Link Management.

is shown in Figure 4-6.The

4.0 MAGIC INTERFACE DEFINITIONS

ARINC SPECIFICATION 839-Page 55

MIH User

Media-Independent

Interface

MH_5AP

MIHF

MIHLNET SAP

MHLNET SAP MIHF

MIH_LINK_SAP

Media-Dependent
Interface

Link

Figure 4-6:IEEE 802.21-2008 MIHF Reference Model

COMMENTARY

MAGIC-to-MAGIC wire-level protocol
It may be desirable to define messages similar to those defined in
Section 8 of IEEE 802.21.RFC 5677 is referenced for message
flows.

is not defined by IEEE 802.21.

4.2.2 Common Link Interface Primitives

The command and event service primitives are listed below:

·
·

·

·

·
·

·

Link_Parameters_Report
Link_Action(Modified to be Link_Resource-See below)

Link_Down

Link_Going_Down

Link_Get_Parameters
Link_Detected

Link_Up

Additional primitives are required for managing the services provided by the
Common Link Interface:

·

·

·

Link_Capability_Discover

Link_Event_Subscribe

Link_Event_Unsubscribe

The command,event and service management primitives are identical to the
MIH_LINK_SAP primitives specified in IEEE 802.21-2008.

Link_Resource is a new command service primitive for resource allocation and
release for a specific link.This primitive is not specified in IEEE 802.21 and is
specified by MAGIC in the next section.

ARINC SPECIFICATION 839-Page 56

4.2.2.1

Link_Resource

4.0MAGIC INTERFACE DEFINITIONS

The Link_Action primitive is not suited for requesting or releasing resources on a
link as the parameters specified in IEEE 802.21-2008 do not cover QoS related
information.The Link_Resource primitive replaces the Link_Action primitive.The
following primitives are added for this purpose:

Type
Description

Parameters in Request
Data Types in Request
Parameters in Confirm
Data Types in Confirm

Command
Used to request or release resources on a link.Except for the
Linkldentifier,the parameters used in this primitive are
equivalent to those in the MIH_Link_Resource primitive.
Bearerldentifier,QoSParameters
BEARER_ ID, QOS_ PARAM
Bearerldentifier
BEARER_ID

The parameter settings for requesting additional resources on a new bearer are as

follows:

ResourceAction

= 1,QoSParameters={ …}

The parameter settings for

requesting additional

resources on an existing bearer are

as

follows:

ResourceAction

= 1,Bearerldentifier=Y,QoSParameters={ …}

The parameter settings for releasing resources on a specific bearer are as follows:

ResourceAction

=0,Bearerldentifier

=Y

The fullspecification of this new primitive is provided in Attachment 2.

COMMENTARY

4.3 Aircraft

Interface

4.3.1 Time Format and Synchronization

Onboard and ground systems have several reasons to share a common time
reference.Further,onboard clients of MAGIC require a common time reference to
issue and process Diameter requests.Examples are:

·

·

·

Cost accounting between systems where start and stop times are

asynchronous to signaling

QoS profiles requiring network paths through multiple intermediaries where
packet transit times cannot be monitored on a 'send to get'mode

Tracking of expiration for communication requests where a time-to-live is
specified by an external system

Coordinated Universal Time (UTC)should be employed by allaircraft and ground
MAGIC implementations for commonality of reference.Proposed below is the three-
tiered time synchronization approach.

4.0 MAGIC INTERFACE DEFINITIONS

ARINC SPECIFICATION 839-Page 57

4.3.1.1 Central Time Source

An MAGIC installation can rely upon the aircraft's infrastructure to utilize a stable
time source providing UTC formatted time.The accuracy of this source is dependent
on the source and external means of synchronization.A prevalent example onboard
would be usage of avionics time,often distributed to the pilot and maintenance
displays and included in aircraft fault logs and in-service records.

This model may present two issues if deployed:accuracy relative to ground
installations or service providers and unavailability of a time reference when aircraft
is in maintenance mode or experiencing a failure.

4.3.1.2 MAGIC Ground-to-Air Time Servicing

As a supplement,or possible replacement to,the Aircraft central time service would
be a network centric approach for the onboard MAGIC time source.Using an
existing,standardized interface to transmit accurate UTC time information from any
connected MAGIC entity on the ground can allow movement of time information to
the aircraft to prevent dilation between MAGIC services.

4.3.1.3 Network Time Protocol(NTP)

This is the proposed standard for MAGIC services to retrieve and coordinate the
working UTC system time. NTP version 4 is currently standardized by RFC 59 0 5.
Using this protocol,MAGIC would communicate under the clientlserver model as a
client to a public time server in the cloud.

This strategy can be expanded onboard within a Federated MAGIC implementation
where-by a single MAGIC entity established an NTP connection to a discovered
time server from a public pool,after which,the time server communicates to the
other MAGIC entities.Each MAGIC entity would then serve as both a client(when
communicating to the ground)and possibly as a lower stratum server when
requested by peer onboard systems.This model reduces the total amount
overhead by eliminating the need to have several onboard systems synchronizing
with ground time sources.

IP traffic

4.3.1.4 Recommended Time-Protocol

Implementation

The following is the proposed standard implementation for MAGIC services:

Onboard installations:

1. Rely first on the aircraft common sourced UTC time.
2.Second,attempt

to utilize an IP link into the MAGIC ground entities to
negotiate a User Datagram Protocol (UDP)based NTP request for the
accurate UTC time.

3.Finally,if an aircraft time is not sourced and if no links are available,the

system resorts to an internal persistent clock.

4.Degraded mode is entered when no time source is available,inhibiting all

functions which rely on UTC commonality with external systems.Operation
of non-UTC dependent functions are continued to be executed or served,
using a 'start at zero'internal clock where a time value is required,but not
critical.

ARINC SPECIFICATION 839-Page 58

4.0 MAGICINTERFACE DEFINITIONS

Ground Systems:

1.Local server time can be instantiated at start-up for MAGIC to communicate

with onboard systems.

2.NTP based requests to hosted time servers should continue,as appropriate,

based on the accuracy of the hosting system.

4.3.2 Aircraft State Information

Various aircraft systems are required to communicate with MAGIC to affect the
routing of real-time information.The specifics of the parameters required from the
aircraft systems for effective message routing and the means by which these
parameters are sourced to MAGIC are both beyond the scope of this standard.
MAGIC is expected to support ARINC Specification 834 as the defining document
for the mechanisms for access to aircraft parameters.

The parameters which should be taken into consideration during the design of a
MAGIC enabled aircraft are listed below.All parameters listed below are considered
as optional

input to MAGIC.

MAGIC uses the available parameters in its profiles to define communication
selection rules.

Type

Description

Weight on Wheels
(WoW)
Position
Current Flight Phase
Aircraft Registration

Altitude

Parameters
Various aircraft systems are required to communicate with
MAGIC to effect the routing of real-time information
The Weight on Wheels (WoW)parameter is an indication that
the aircraft is on the ground
The Position is the geospatial location of the aircraft
Current Phase of Flight(e.g., cruise)
String containing a(as unique as possible)identifier of the
aircraft
Numeric value in feet

4.4 Configuration and Administration

MAGIC is based upon the operating principle that preconfigured rules are controlling
the boundaries of what clients can do in terms of bandwidth negotiation and their
offered services.The configuration management of these rules is fundamental to
correct MAGIC operation.However,this version of the standard does not specify
this configuration.

Typical avionics and aircraft systems are loaded with software and configuration
data using the ARINC Report 665 formats.These formats,along with onboard and
portable data loaders,provide a tested mechanism for airlines to manage and
support avionics in the field.For MAGIC,this concept ensures that no new process
or technology is loaded on airlines without airline and their regulatory authority
approval.

Since MAGIC is a function within complex network environment,there is the
opportunity for introducing a format to configure,manage,and update aspects of
MAGIC's configuration.

4.5 Service-Provider Versus Airline-Hosted Ground Peer Function

This function has been deferred to a future supplement to this standard.

4.0 MAGIC INTERFACE DEFINITIONS

ARINC SPECIFICATION 839-Page 59

4.6 Aircraft- Ground Management

The protocol and its message formats are deferred to a future supplement of this
standard.

ARINC SPECIFICATION 839-Page 60

5.0 NETWORK MAINTENANCE,LOGGING,AND SECURITY REQUIREMENTS

5.0 NETWORK MAINTENANCE,LOGGING,AND SECURITY REQUIREMENTS

MAGIC includes mechanisms for collecting and storing information about its
operation and resources.This information may be exchanged with and/or reported
to other onboard systems for the purposes of monitoring,controlling,and/or
coordinating MAGIC resources.The network management

functions include:

·

·

·

·
·

(AM)-Collection of MAGIC resource usage

Accounting Management
information.
Configuration Management (CM)-Management and configuration of
hardware,software,and database components.
Fault Management
diagnostics,and fault

(FM)-Fault surveillance,alarm reporting,integrated
recovery.

Performance Management (PM)-Collection of performance data.
Security Management
controls.

(SM)-Monitoring and enforcement of security

5.1 Accounting Management(AM)Requirements

5.1.1 Usage Records

MAGIC must have the capability to generate a usage record based on information
collected for each air-ground communication session.

MAGIC shall as a minimum record the following parameters:

● Date/Time
Session ID
·

● Source
·
·

Destination
Payload size

·

·

Link

establishment,selection,setup,and termination

Client-related parameters

· Other User-selectable

5.2 Configuration Requirements and Recommendations

5.2.1 System Clock

MAGIC shall automatically synchronize its system clock to UTC time.

5.2.2 Computing and Data Storage Resources

This standard is written with the assumption that MAGIC operates on a system that
has sufficient computing and storage resources to meet the intended function.This
implies that:

·

·

MAGIC shall have access to means of persistent storage for network
management data including logs (e.g., process activity,faults)and records
(e.g.,usage and performance).
MAGIC shall have access to persistent configuration data for system
initiation,including Client Profiles,Link Profiles,and Central Policy Profile.

ARINC SPECIFICATION 839-Page 61

5.0 NETWORK MAINTENANCE,LOGGING,AND SECURITY REQUIREMENTS

5.3 Security Management Requirements
MAGIC shall
management data to authorized entities as required by the governing network
security architecture.

include security controls that limit access to MAGIC network

5.3.1 Security Event Logging

logging must be provided to the highest degree necessary to satisfy the

Event
system risk assessment.The following material
Document TAB-35A:Considerations for the Incorporation of Cyber Security in the
Development of Industry Standards.

is an excerpt from ARINC

MAGIC shall create security log events,including as a minimum:

1.Authentication
2.Loading of new security and or configuration data(e.g.,policies)

failures

3.Disabling/enabling of

interfaces or functions

4.Denied access errors
5.All events requiring privileged access

It is assumed that the governing network security logging architecture willdefine the
information to be included into each security log entry and the MAGIC
implementation will conform to it.

ARINC SPECIFICATION 839-Page 62

ATTACHMENT 1
CLIENTINTERFACE PROTOCOL DETAILS

ATTACHMENT 1

CLIENT INTERFACE PROTOCOL DETAILS

1.1 Diameter Extensions and Modifications

1.1.1 Attribute Value Pairs

1.1.1.1 Session Related AVPs

1.1.1.1.1 Session-Id AVP

The Session-ld AVP is used to identify a specific session.All messages pertaining
to a specific session shall
the lifetime of a session.

include only one Session Identifier that

is used throughout

Within Diameter the Session-Id AVP is owned and generated by the client and is not
assigned by the server.In order
double IDs,MAGIC may overwrite the Session ID requested by the client.If
no ID conflict,MAGIC uses and responds back the Session-ld AVP generated by
the client.The format of

to have unique Session identifiers and to avoid

this AVP remains unchanged.

there is

1.1.1.1.2 Session-Timeout AVP

The Session-Timeout AVP is stated as an optional
Protocol stack,but within MAGIC this field is required in the response to the clients.
The format of

this AVP remains unchanged.

field of the Diameter Base

1.1.1.1.3 Auth-Session-State AVP

As MAGIC accommodates multiple connections of multiple clients via different
communication links,MAGIC is required to maintain the states of all
at all

times.

the connections

A MAGIC server shallonly use 0 (State Maintained)in the Auth-Session-State AVP
answers to a client.The Auth-Session-State AVP is redefined as a required item in
all commands that are sent from the server to a client using this AVP.The format of
this AVP remains unchanged.

1.1.1.2 Authentication AVPs

1.1.1.2.1 Client-Password AVP

The Client-Password AVP defines the pass phrase belonging to a client
to register at
within MAGIC.

to be able
the MAGIC services.This password is also stored in the client-profile

AVP-Name:Client-Password
AVP-Code: 10001
AVP-Type: UTF8String

1.1.1.2.2 Server-Password AVP

The Server-Password AVP defines the pass phrase of MAGIC that may be used by
the client

for authenticating the MAGIC server.

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

ARINC SPECIFICATION 839-Page 63

AVP-Name:Server-Password
AVP-Code:10045
AVP-Type: UTF8String

1.1.1.3 Status Information AVPs

1.1.1.3.1 REQ-Status-Info AVP

If the REQ-Status-Info AVP is present,the client is requesting MAGIC to send
Status Change Reports to the client.An example would be a logging client.The
flags define what status information is to be sent to the client.The flags can be
ordered together to request multiple status information at the same time.

MAGIC shall verify whether the client is authorized to receive the requested

information.

AVP-Name:REQ-Status-Info
AVP-Code: 10002
AVP-Type: Unsigned32 with the following values:

0=No_Status
1=MAGIC_Status
2=DLM_Status
3=MAGIC_DLM_Status
6=DLM_LINK_Status
7=MAGIC_DLM_LINK_Status

The following values are defined for this AVP:

Value
0

1

2

3

6

7

ID

No_Status

MAGIC_Status

DLM_Status

MAGIC_DLM_Status

DLM_Link_Status

MAGIC_DLM_LINK_Status

Description

No

information needed.

General status information about the
MAGIC system is needed.
Basic status information about the
communication links is needed
only).

(DLM-Level

General status information about the
MAGIC system and basic link status
information types are needed
only).

(DLM-Level

Detailed status information about the
communication links is needed
and associated links) .

(DLM-level

General status information about the
MAGIC system and detailed status
information about the communication links
is needed
links).

(DLM-level and associated

1.1.1.3.2 Status-Type AVP Provided by MAGIC

The Status-Type AVP has the same format as the REQ-Status-Info AVP.MAGIC
indicates to the client which status information is provided within the response.

ARINC SPECIFICATION 839-Page 64

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

AVP-Name:
AVP-Code:
AVP-Type:

Status-Type
10003
Unsigned32 with the following values:
0=No_Status
1=MAGIC_Status
2=DLM_Status
3=MAGIC_DLM_Status
6=DLM_LINK_Status
7=MAGIC_DLM_LINK_Status

The following values are defined for this AVP:

Value
0

1

2

3

6

7

ID

No_Status
MAGIC_Status

DLM_Status

MAGIC_DLM_Status

DLM_Link_Status

MAGIC_ DLM_ LINK_ Status

Description

No information needed.
General status information about the
MAGIC system is needed.
Basic status information about the
communication links is needed (DLM-Level
only).
General status information about the
MAGIC system and basic link status
information types are needed
only).
Detailed status information about the
communication links is needed (DLM-level
and associated links).

(DLM-Level

General status information about the
MAGIC system and detailed status
information about the communication links
is needed
links).

(DLM-level and associated

1.1.1.4 Data Link Module(DLM)Related AVPs

1.1.1.4.1 DLM-Name AVP

The DLM-Name AVP contains a name in form of a string to be able to refer to a
particular service of a link controller(handled via a DLM).This string is also used
elsewhere within the MAGIC system(i.e.,profiles).

AVP-Name:DLM-Name
AVP-Code:
AVP-Type:

10004
UTF8String

1.1.1.4.2 DLM-Available AVP

This information is queried from the link controller in regular intervals.Not all
controllers support this query.Moreover,YES does not
open a link successfully.It is a mere indication that the hardware can be used.

indicate that

it

link

is possible to

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

ARINC SPECIFICATION 839-Page 65

AVP-Name: DLM-Available
AVP-Code:
AVP-Type: Enumerated with the following values:

10005

1=YES
2=NO
3 = UNKNOWN

The following values are defined for this AVP:

Value

ID

Description

0

1

2

YES

NO

UNKNOWN

The DLM is reachable and can be interfaced to,but
this does not indicate if services are available.
The DLM is not reachable,but this does not indicate
if the DLM is accidently not available or if it is due to
restrictions (i.e.,not allowed in a particular flight-
phase).

MAGIC was not able to get information of the link
controller(via this DLM)and is not able to state the
availability of the DLM.

1.1.1.4.3 DLM-Max-Bandwidth AVP

With this AVP,MAGIC informs the client of the maximum available bandwidth that a
specific link is capable of providing in kilobits per second (kbps).This is theoretical
maximum throughput(e.g.,64
Digital Network
(ISDN)channel within

kbps per mobile
Inmarsat Satellite).

Integrated Services

an

AVP-Name:DLM-Max-Bandwidth
AVP-Code:10006
AVP-Type: Float32
AVP-Unit:

kbps

1.1.1.4.4 DLM-Allocated-Bandwidth AVP

This AVP indicates the allocated bandwidth in kbps.This is the bandwidth used by

all

clients.

AVP-Name: DLM-Allocated-Bandwidth
AVP-Code: 10007
AVP-Type: Float32
AVP-Unit:

kbps

1.1.1.4.5

DLM-Max-Return-Bandwidth

AVP

This AVP indicates the maximum theoretical

return bandwidth in kbps to this aircraft

(shared to all clients).If

this AVP is not present,the allocation is symmetric between

forward and return direction.

ARINC SPECIFICATION 839- Page 66

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

AVP-Name: DLM-Max-Return-Bandwidth
AVP-Code:10008
AVP-Type:
AVP-Unit:

Float32
kbps

1.1.1.4.6 DLM-Allocated-Return-Bandwidth AVP

This AVP indicates the return bandwidth in kbps that has been allocated to all
clients.If this AVP is not present,the allocation at the link is done symmetrically and
is equal on forward and return path.

AVP-Name: DLM-Allocated-Return-Bandwidth
AVP-Code:
AVP-Type:
AVP-Unit:

10009
Float32
kbps

1.1.1.4.7 DLM-Max-Links AVP

Maximum number of available links(e.g.,22 in an ARINC781 two-channel-card
swift-broadband modem).

AVP-Name:DLM-Max-Links
AVP-Code:
AVP-Type:

10010
Unsigned32

1.1.1.4.8 DLM-Allocated-Links AVP

This AVP indicates the already allocated number of links.The remaining links are

DLM-Max-Links minus DLM-Allocated-Links.

AVP-Name:DLM-Allocated-Links
10011
AVP-Code:
Unsigned32
AVP-Type:

1.1.1.5 Detailed Link Information Related AVPs

1.1.1.5.1 Link-Number AVP

The Link-Number AVP refers to the modem internal sub-link if available (i.e., Packet
Data Protocol (PDP)context within an ARINC781 modem).

AVP-Name:Link-Number
AVP-Code:10012
AVP-Type:

Unsigned32

1.1.1.5.2 Link-Name AVP

The Link-Name AVP provides a description of the link(e.g.,"Streaming-32"for a
SwiftBroadband PDP-Context).

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

ARINC SPECIFICATION 839-Page 67

AVP-Name:Link-Name
AVP-Code:10054
AVP-Type: UTF8String

1.1.1.5.3 Link-Available AVP

This AVP indicates whether the link that

is

referred with the Link-Number AVP is

available or not.

AVP-Name:Link-Available
AVP-Code:
AVP-Type: Enumerated

10013

1=YES
2=NO

1.1.1.5.4 Link-Connection-Status AVP

This AVP indicates whether the link that

is

referred with the Link-Number AVP is

connected or not.

AVP-Name:Link-Connection-Status
AVP-Code:10014
AVP-Type: Enumerated

1=Disconnected
2= Connected
3= Forced_Close

The following values are defined for this AVP:

Value

ID

0

1

2

Disconnected

Connected

Forced_Close

Description
The link referred to is disconnected and cannot be
used for communication until it has been opened.
The link referred to is connected and can be used for
communications.
MAGIC has been forced to close the link by a third
party (such as a service provider).

1.1.1.5.5 Link-Login-Status AVP

This AVP indicates whether a modem is logged in to the network for

the particular

link referred in the Link-Number AVP.

AVP-Name:Link-Login-Status
AVP-Code:10015
AVP-Type:Enumerated
1=Logged off
2=Logged on

1.1.1.5.6

Link-Max-Bandwidth

AVP
With this AVP,MAGIC informs the client of

the maximum available bandwidth in

kbps of the specific link referred to in the Link-Number AVP.This bandwidth is the

ARINC SPECIFICATION 839-Page 68

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

theoretical maximum throughput

(e.g.,64 kbps of a mobile ISDN channel within an

Inmarsat

Satellite).

AVP-Name: Link-Max-Bandwidth
AVP-Code: 10016
AVP-Type: Float32
AVP-Unit:

kbps

1.1.1.5.7 Link-Max-Return-Bandwidth AVP

This AVP indicates the maximum theoretical
particular
allocation is symmetric between forward and return direction and is also captured by

link referred to in the Link-Number AVP.If

this AVP is not present,the

return bandwidth in kbps of

the

the

Link-Max-Bandwidth AVP.

AVP-Name: Link-Max-Return-Bandwidth
AVP-Code: 10017
AVP-Type: Float32
AVP-Unit:

kbps

1.1.1.5.8 Link-Alloc-Bandwidth AVP

With this AVP,MAGIC informs the client of
for the specific link referred to in the Link-Number AVP.

the currently allocated bandwidth in kbps

AVP-Name: Link-Alloc-Bandwidth
AVP-Code:
AVP-Type: Float32
AVP-Unit:

kbps

10018

1.1.1.5.9

Link-Alloc-Return-Bandwidth

AVP

the currently allocated return bandwidth in
the specific link referred to in the Link-Number AVP is capable of offering.

With this AVP,MAGIC provides
kbps that
If
direction and is also captured by the Link-Alloc-bandwidth AVP.

this AVP is not present,the allocation is symmetric between forward and return

the client

AVP-Name:Link-Alloc-Return-Bandwidth
AVP-Code: 10019
AVP-Type: Float32
AVP-Unit:

kbps

1.1.1.5.10

Link-Error-String AVP

If

this AVP is present

it

indicates that

there was an error

related to the DLM/Link.

The AVP contains error

information of

the last error applicable to this DLM/Link.

ATTACHMENT 1
CLIENTINTERFACE PROTOCOL DETAILS

ARINC SPECIFICATION 839-Page 69

AVP-Name:Link-Error-String
AVP-Code:10020
AVP-Type: UTF8String

1.1.1.6 Communication Related AVPs

1.1.1.6.1 Throughput Related AVPs

1.1.1.6.1.1 Requested-Bandwidth AVP

A client with communication requirements shall use the Requested-Bandwidth AVP
the desired requirements for communicating to the ground.
to inform MAGIC about
This AVP is specified in kbps.It
if the Requested-
Return-Bandwidth AVP is not present and as a request

is treated as a symmetric request

to the forward direction

(aircraft-to-ground)if

the Requested-Return-Bandwidth AVP is present.

AVP-Name: Requested-Bandwidth
AVP-Code: 10021
AVP-Type: Float32
AVP-Unit:

kbps

This value contains the current assigned bandwidth for this session in the MAGIC-
Notification-Report(MNTR).If MAGIC is not able to provide this throughput,MAGIC
answers with a lower value that MAGIC is able to offer.The client can then
determine if the offered value is sufficient to run the session or not.

1.1.1.6.1.2 Requested-Return-Bandwidth AVP

A client with asynchronous communication requirements shall use the Requested-
Return-Bandwidth AVP to inform MAGIC about
the desired requirements for the
backward communicating direction between aircraft and ground(i.e.,ground-to-
aircraft

traffic) .

This AVP is specified in kbps.MAGIC treats the communication requirements used
the Requested-Return-Bandwidth
in the Requested-Bandwidth AVP as symmetric if
AVP is not present.

AVP-Name: Requested-Return-Bandwidth
AVP-Code:10022
AVP-Type: Float32
AVP-Unit:

kbps

This value contains the current assigned bandwidth for this session in the MAGIC-
Notification-Report(MNTR).

1.1.1.6.1.3 Required-Bandwidth AVP

The Required-Bandwidth AVP offers a client
required throughput
that
this value cannot be met by MAGIC the link is not established and the request
rejected.If
throughput,MAGIC confirms this value with the old value from an earlier
higher
request(provided in a client authentication request or a communication change

the value was included in a communication change request

is needed to maintain the communication link to ground.If

the capability to specify the minimum

to request a

is

ARINC SPECIFICATION 839-Page 70

ATTACHMENT 1
CLIENTINTERFACE PROTOCOL DETAILS

request).The client can then determine whether

to retain the old values or

terminate

the

communication.

This AVP must occur lower in the Requested-Bandwidth AVP if
request.

it

is provided in a

In any other case (AVP not
Requested-Bandwidth AVP)defaults to the value of Requested-Bandwidth AVP.

include or AVP value higher than the value of

This AVP is treated as a symmetric request

if

the REQURED-RETURN-

BANDWDTH AVP is not present otherwise this AVP is only valid for the forward
direction

(aircraft-to-ground).

AVP-Name:Required-Bandwidth
AVP-Code:10023
AVP-Type: Float32
AVP-Unit:

kbps

This value contains the current assigned bandwidth for this session in the MAGIC-
Notification-Report

(MNTR).

1.1.1.6.1.4 Required-Return-Bandwidth AVP

the capability to provide the

The Required-Return-Bandwidth AVP offers a client
minimum required backward throughput
(ground-to-aircraft)that
maintain the communication link between aircraft and ground.If
met by MAGIC,the link is not established and the request
was included in a communication change request
MAGIC confirms this value with the old value from an earlier
client authentication request or a communication change request). The client can
terminate the communication.
then determine whether

the value
throughput,
(provided in a

is needed to
this value cannot be

to retain the old values or

to request a higher

is rejected.If

request

This AVP must occur lower
it is provided in a request.

in the REQUESTED-BACKWARD-BANDWIDTH AVP if

In any other case(AVP not
REQUESTED-BACKWARD-BANDWIDTH AVP)defaults to the value of

included or AVP value higher than the value of

REQUESTED-BACKWARD-BANDWIDTH

AVP.

The REQUESTED-BACKWARD-BANDWIDTH AVP is treated as a symmetric
request

if this AVP is not present.

AVP-Name:Required-Return-Bandwidth
AVP-Code:10024
AVP-Type: Float32
AVP-Unit:

kbps

This value contains the current assigned bandwidth for this session in the MAGIC-
Notification-Report

(MNTR)

1.1.1.6.1.5 Granted-Bandwidth AVP

The Granted-Bandwidth AVP is used by the MAGIC server to inform the client about
the throughput received from the Link Controller.The value of
Bandwidth AVP is between the value of

the Requested-Bandwidth AVP and the

the Granted-

ARINC SPECIFICATION 839-Page 71

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

Required-Bandwidth AVP sent
Group.

in the Communication-Request-Parameter AVP-

This AVP is treated as a symmetric request if the Granted-Return-Bandwidth AVP is
not present otherwise this AVP is only valid for the forward direction (aircraft-to-
ground).

AVP-Name:Granted-Bandwidth
AVP-Code :10051
AVP-Type:
AVP-Unit:

Float32
kbps

1.1.1.6.1.6 Granted-Return-Bandwidth AVP

The Granted-Return-Bandwidth AVP is used by the MAGIC server to inform the
client about the asymmetric throughput received from the Link Controller.If this AVP
is present,it represents the ground-to-aircraft direction.

AVP-Name:Granted-Return-Bandwidth
AVP-Code:10052
AVP-Type:
AVP-Unit:

Float32
kbps

1.1.1.6.2 Prioritization and QoS AVPs

1.1.1.6.2.1 Priority-Class AVP

The sessions with the highest Priority Class are evaluated first for bandwidth
assignment.Sessions with lower Priority Class receive only the bandwidth that
remains after all sessions in higher Priority have their requested bandwidth.The
possible Priority-Class AVP entries are defined in MAGICs global configuration file.
Client defined attribute is used to classify the relative priority of the traffic associated
with this session.Boundaries of what a client can specify are defined within the
MAGIC configuration files.

AVP-Name:Priority-Class
AVP-Code :
AVP-Type:

10025
UTF8String

The configuration of the priority classes is dependent on the QoS-Level AVP and
the boundaries should follow IETF RFC 32 60 " New Terminology and Clarifications
for Diffserv."

1.1.1.6.2.2 Priority-Type AVP

The Priority-Type AVP categorizes the traffic and indicates to the MAGIC Server
how to treat this session's traffic relative to all other traffic from the client.The Value
can either be BLOCKING,which causes traffic of a lower priority to be dropped in
MAGIC,or PREEMPTION,which has precedence against traffic of a lower priority.
The default value is PREMPTION." Blocking"forces other traffic with lower
PRIO_CLASS to be dropped in MAGIC;"Pre-emption"traffic has precedence
against

traffic in lower PRIO_CLASS for bandwidth assignment(default).

ARINC SPECIFICATION 839-Page 72

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

A“Blocking”session stops ALL other
only affect a single link or the requested bandwidth.An example is the
EMERGENCY class.MAGIC checks,if

the client has the right

lower priority traffic.Such a session does

to block other

not

traffic.

AVP-Name:Priority-Type
AVP-Code: 10026
AVP-Type: Enumerated
1=Blocking
2= Preemption

1.1.1.6.2.3 QoS-Level AVP

This AVP provides a list of supported QoS levels.A QoS level defines the service
level a link controller can offer.Each DLM maps the three QoS Levels onto an
appropriate link
their
link
level supported requires a single QoS-Level AVP.

these three levels to be transparent.Each QoS

services,MAGIC provides

service.Because different

controllers use different

names

link

for

AVP-Name:QoS-Level
AVP-Code:10027
AVP-Type: Enumerated

0=BE
1=AF
2=EF

The following values are defined for this AVP:

Value

ID

0

1

2

BE

AF

EF

Description
Best Effort (you get what is available with no
guaranty)
Assured Forwarding (gives assurance of delivery
under prescribed conditions that is further described
in IETF RFC 2597 and IETF RFC 3260)
Expedited Forwarding (dedicated to low-loss,low-
latency traffic that is further described in IETF RFC
3246)

1.1.1.6.3 Routing and

Link Selection Related AVPs

1.1.1.6.3.1

DLM-Availability-List

AVP

is considered a blacklist;otherwise,it

This AVP forms a list of Data Link Module Names (DLMs).The DLM names are
given as a comma separated list.If the list starts with the keyword "not"followed by
a space,it
interpreted to be in preference order.That
DLM.This list
is available for
AVP is not provided.

is a white list.The white list
is,the first entry is the most preferred

that client.MAGIC uses the client profile list

the selected DLM
the DLM if

the client profiles to insure that

is compared against

to select

this

is

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

ARINC SPECIFICATION 839-Page 73

AVP-Name:DLM-Availability-List
AVP-Code:10028
AVP-Type: UTF8String

1.1.1.6.3.2 Gateway-IPAddress AVP

After a link has been established successfully this AVP is used by MAGIC to inform

the client about the IP Addressed at the link.It can be an IPv4 or an IPv6 address.

AVP-Name:Gateway-IPAddress
AVP-Code:10029
AVP-Type: UTF8String

1.1.1.6.3.3 Traffic Flow Template AVPs

In contrast to the specified behavior in the [DBP-RFC],a session may consist of
more than one connection.All users(actually,their connections)of a WLAN access
point may be considered a single session.Therefore,the payload of a single
session can be all packages sent from a range of source IP addresses to a range of
IP destinations(and port
The Traffic Flow Template(TFT)describes these IP and port
specifies which package belongs to this session.

ranges respectively).

ranges and thus

There must be at least one TFT for each session and up to 255 TFTs can be used
to specify the packages belonging to each session.

The number of TFTs per session shall be as small as possible due to impact on
performance of the system.

The TFT is contained as a string.This string is specified in a separate section of this
appendix.

COMMENTARY
A profile can contain TFTs,so that a client does not have to address
them via interface calls.

Two kinds of TFTs are defined in the interface,one from Aircraft-to-Ground and one

from Ground-to-Aircraft.

1.1.1.6.3.3.1 TFTtoGround-Rule AVP

This AVP defines the Traffic Flow Templates in the forward direction(for traffic from
aircraft-to-ground).

AVP-Name:TFTtoGround-Rule
AVP-Code: 10030
AVP-Type: UTF8String

1.1.1.6.3.3.2 TFTtoAircraft-Rule AVP

This AVP defines the Traffic Flow Templates in the backward direction(for traffic
from ground-to-aircraft).

ARINC SPECIFICATION 839-Page 74

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

COMMENTARY

The forward TFTs are used and reversed to allow the backward
traffic as well,so that this TFTs only should show up for Traffic that is
not captured as return traffic from already existing forward TFTs.

AVP-Name:TFTtoAircraft-Rule
AVP-Code:10031
AVP-Type:

UTF8String

1.1.1.6.3.3.3 NAPT-Rule AVP

The NAPT-Rule AVPs provide a list of Network and Port Address Translation
(NAPT)entries.

MAGIC must do a translation of IP addresses and ports between the aircraft and the

link controller.

These entries can be used to enforce a session specific mapping.There is one
NAPT-Rule AVP present per NAPT string.

For the complete description of the string format refer to the NAPT string definition

in this attachment.

AVP-Name:NAPT-Rule
AVP-Code:10032
AVP-Type:

UTF8String

1.1.1.6.4 Location and Aircraft Parameter Related AVPs

1.1.1.6.4.1 Flight-Phase AVP

A client and MAGIC can exchange the flight phases for which this session is to be
used.The value is a comma delimited string of the following possible values (GATE,
TAXI,TAKE_OFF,CLIMB,CRUISE,and

DESCENT).

AVP-Name:Flight-Phase
AVP-Code:10033
AVP-Type:

UTF8String

1.1.1.6.4.2 Altitude AVP

This AVP defines client defined altitude ranges during which this session is to be
active.These altitude ranges are a list of comma separated values.If the list starts
with a "not"and a space,this is considered a black list;otherwise,it
session can only be active as long as the aircraft is in a permitted altitude.

is a white list.A

The format of the Altitude AVP is as follows:

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

ARINC SPECIFICATION 839-Page 75

AVP-Name:Altitude
AVP-Code:10034
AVP-Type: UTF8String

The default is all altitudes.The format of each range is:“<from>'-'<to>.”One of the
two values may be omitted.The following examples show how the string can be
formatted:

1.1000-2000
2.not 1000-2000,20000-
3.-5000
4.20000-

1.1.1.6.4.3 Airport AVP

This client defined Airport AVP is a black list or white list of airports where sessions
are to be active.The values are a comma separated list of airports.If the list starts
with a "not"keyword and a space,this is considered a black list;otherwise,it is a
white list.The entries are three-character airport codes(like "MUC"for Munich).If
this is a white list,the session can only be active at a listed airport.If this is a
blacklist the session can only be active at an airport not in the list.The default is the
session can be active at all airports.

AVP-Name:Airport
AVP-Code: 10035
AVP-Type: UTF8String

1.1.1.6.5 Other AVPs

1.1.1.6.5.1 Accounting-Enabled AVP

A value of 0 means accounting is disabled,any other value means accounting is
enabled.The default is to enable accounting.Clients need the ability for their client
profiles to disable accounting.

AVP-Name:Accounting-Enabled
AVP-Code: 10036
AVP-Type: Unsigned32

1.1.1.6.5.2 Keep-Request AVP

This AVP enumerates whether or not a request should be discarded when an
assignment could not be granted.The default value is NO.A value of YES indicates
that a MAGIC server continually attempts to assign the requested bandwidth to the
session.It even re-opens the session automatically,if for any reason the session
was interrupted.

ARINC SPECIFICATION 839-Page 76

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

AVP-Name: Keep-Request
AVP-Code:
AVP-Type: Enumerated

10037

0=NO
1=YES

1.1.1.6.5.3 Auto-Detect AVP

If the Auto-Detect AVP is not set
bandwidth but waits until
appropriate bandwidth.The first

it

to "NO,"MAGIC does not
this

receives traffic for

session and then assigns

immediately assign

few packets may be lost.

If Yes-Asymmetric is set,MAGIC may set different bandwidths for
forward

path.

the return and

AVP-Name: Auto-Detect
AVP-Code:
AVP-Type: Enumerated

10038

0=NO
1=YES-Symmetric
2= YES- Asymmetric

1.1.1.6.5.4

Timeout AVP

This AVP defines the seconds after which a link with no traffic is automatically
terminated.A value of 0 indicates that no timeout
enforces a default value for

this parameter.

is expected that MAGIC

is set.It

AVP-Name: Timeout
AVP-Code:
AVP-Type: Unsigned32
AVP-Unit:

Seconds

10039

1.1.1.7 Profile related AVPs

1.1.1.7.1

Profile-Name

AVP

Profiles are XML files stored in the configuration directory of MAGIC.These files
contain the same fields as the communication change request.The values defined
in the given profile should be used for all parameters that are not defined in this
request.That
the parameters in this message overwrite the profile ones.

is,if a profile is used and some parameters are given in this message,

AVP-Name:Profile-Name
AVP-Code:10040
AVP-Type: UTF8String

1.1.1.7.2

Registered-Clients AVP

The Registered-Clients AVP forms a comma separated list of all clients,which are
currently active

sessions with MAGIC).

(i.e.,have ongoing

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

ARINC SPECIFICATION 839-Page 77

AVP-Name:Registered-Clients
AVP-Code: 10041
AVP-Type: UTF8String

1.1.1.8 Accounting Related AVPs

1.1.1.8.1 Accounting Information AVPs

1.1.1.8.1.1

CDR-Type AVP

This AVP specifies how accounting data is to be received.Depending on the given
type,the appropriate AVPs have to be established to specify the parameter

necessary

for

the request.

AVP-Name:CDR-Type
AVP-Code:
AVP-Type: Enumerated

10042

1=LIST_REQUEST
2 = DATA_ REQUEST

A LIST_REQUEST asks for a list
while a DATA_REQUEST causes

1.1.1.8.1.2

CDR-Level AVP

containing the identifiers for
the transmission of

the stored CDRs,

the actual accounting data.

The CDR-Level specifies on what basis the requested list
the CDR-TYPE AVP is set
only relevant
ALL,a list of

if
identifiers for all CDRs currently stored is provided.

to LIST_REQUEST.If

is generated.This AVP is
to

the value

is set

AVP-Name: CDR-Level
AVP-Code:
AVP-Type: Enumerated

10043

1=ALL
2= USER_ DEPENDENT
3=SESSION_DEPENDENT

to USER_DEPENDENT,a list of CDR identifiers is generated for a

the value is set

If
specified user during the current
to
SESSION_DEPENDENT,a list of CDR identifiers for a specified session is
provided.

leg is provided.If

the value is set

flight

1.1.1.8.1.3

CDR-Request-Identifier

AVP

This AVP specifies to the user

that a list of CDR identifiers is generated.

AVP-Name:CDR-Request-Identifier
AVP-Code:10044
AVP-Type: UTF8String

This AVP is only relevant

if

the list CDR-Type AVP is set

to USER_DEPENDENT.

1.1.1.8.1.4 CDR-ID AVP

The CDR-ID AVP contains the accounting identifier.

ARINC SPECIFICATION 839-Page 78

ATTACHMENT 1
CLIENTINTERFACE PROTOCOL DETAILS

To request more than one CDR,this AVP must be included in multiple instances,
each one identifying a single CDR.

AVP-Name:CDR-ID
AVP-Code:10046
AVP-Type: Unsigned32

This AVP is relevant
REQUEST.

1.1.1.8.1.5 CDR-Content AVP

if the CDR-Type AVP is set

to DATA-REQUEST or to LIST-

The CDR-Content AVP contains the content of a call data record belonging to a
CDR-ID AVP.

AVP-Name:CDR-Content
AVP-Code: 10047
AVP-Type: UTF8String

This AVP is only included if the CDR-Type AVP is set

to DATA-REQUEST.

1.1.1.8.2 Accounting Control AVPs

1.1.1.8.2.1 CDR-Restart-Session-ld AVP

This string is the ID of a session.The recording should be stopped and a new
Accounting Record should be created for this session.The identifier of
Accounting Record is given in the MAGIC-Accounting-Control-Answer.

the new

AVP-Name:CDR-Restart-Session-Id
AVP-Code:10048
AVP-Type: UTF8String

1.1.1.8.2.2 CDR-Stopped AVP

The CDR-Stopped AVP indicates the CDR identifier of
Record in case of the request

to restart of a session was successful.

the stopped Accounting

AVP-Name:CDR-Stopped
AVP-Code: 10049
AVP-Type: Unsigned32

1.1.1.8.2.3 CDR-Started AVP

The CDR-Started AVP indicates the CDR identifier of
Record in case of the request

to restart of a session was successful.

the newly created Accounting

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

ARINC SPECIFICATION 839-Page 79

AVP-Name:CDR-Started
AVP-Code: 10050
AVP-Type: Unsigned32

1.1.1.9 Message and Error AVPs

1.1.1.9.1 Error-Message AVP

This Error-Message AVP is an optional field of the basic Diameter protocol stack
and defines a human readable text,describing the error.A single request may
cause several errors.For example,a requester may not get the requested
bandwidth and the requester may not have permission to get the status info.MAGIC
may send an Error-Message AVP for each individual MAGIC-Status-Code that has
been generated.For more details,see also [DBP-RFC].

1.1.1.9.2 MAGIC Status Codes

This value defines the error code,which is supported to the requesting clients.The
integer value,which can be extended with new values in the
error code is a 32 bit
future.

AVP-Name:MAGIC-Status-Code
AVP-Code:10053
AVP-Type: Unsigned32

The following values are defined for this AVP:

Value

ID

1000

MISSING-AVP

1001

AUTHENTICATION-FAILED

1002

UNKNOWN-SESSION

1003

MAGIC-NOT-RUNNING

1004

ENUMERATION-OUT-OF-RANGE

1005

1006

MALFORMED-LINK-STRING

MALFORMED-QOS-STRING

1007

MALFORMED-TFT-STRING

1008

MALFORMED-DATA-LINK-STRING

Description
An important AVP is missing in a
message and it cannot be
processed.
A client did not provide the
correct client name or password.
The session is not known in the
data base. One reason may be
that it has already been
terminated.
Either MAGIC has already
terminated or it has not yet
finished its initialization process.
When reading an integer
representing an enumeration
from a message, it was found to
be out of range.
The LINK string could not be
parsed.
The QoS string could not be
parsed.
The TFT string could not be
parsed.
The DATA- Link string could not
be parsed.

ARINC SPECIFICATION 839-Page 80

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

Value

ID

1009

OPEN-LINK-FAILED

1010

NO-ENTRY-IN-BANDWIDTHTABLE

1011

MISSING-PARAMETER

1012

CLIENT-REGISTRATION

1013

CLIENT-UNREGISTRATION

1014

ILLEGAL-FLIGHT-PHASE

1015

ILLEGAL-PARAMETER

1016

STATUS-ACCESS-DENIED

1017

MAGIC-SHUTDOWN

1.1.2 AVP-Group Definitions

Description
Failed to open link(or additional
link of multilink).
No entry for client in the
bandwidth sharing has been
found.

A request cannot be processed
because a necessary session
parameter has not been
provided.

A new client has opened a
session with MAGIC(actually not
an error but more an
information).
A client has closed its last
session with MAGIC (actually not
an error but more an
information).

The client is not allowed to have
an ongoing session in the current
flight

phase.

The client requested session
parameters that violate the rules
for this client.

The client wanted to get the
global or link status but has no
permission to do so.
MAGIC starts de-initialization.

This section defines various groups that are used to exchange a wider set of
information between the client and MAGIC.Many of these groups do contain
bandwidth related AVPs.For asymmetric link considerations,it
differentiate between two paths,one to the aircraft,and one from the aircraft.The
following terminology defines these considerations:

is necessary to

● Return link:

The transmission link from an aircraft terminal to the central hub on the
ground

● Forward link:

The transmission link from the central hub on the ground to one or more
aircraft

terminals

The following set of definitions provides further clarification:

·

·

Satellite uplink:
The portion of a communications link used for the transmission of signals
from an Earth terminal,in air or on ground,to a satellite

Satellite downlink:
The link from a satellite to an earth terminal,in air or on ground

The two sets of terms are not synonymous.For satellite communications,both the
return link and the forward link require two uplinks and two downlinks,with one
matching pair for each,since each signal must bounce off the satellite before

ARINC SPECIFICATION 839-Page 81

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

reaching a Ground Earth Station(GES),the ground or an Air Earth Station(AES),

and the aircraft.

The default behavior of an ARINC 839 interface may assume that all MAGIC links
are symmetric,assuming equal bandwidth for both the return and forward paths.
is
However,since AES transmission power and antenna size cannot equal what
available on the ground,some links,particularly Ku-band,are asymmetric,offering
more bandwidth on the forward link than on the return link.
Hence,the link request and response parameters need to allow for asymmetric
requests and responses.Currently,these two parameters specify bandwidth.Unless
specified,these parameters are assumed to be symmetric requests,for equal
bandwidth in both return and forward direction.An optional set of parameters,when
provided,make the above parameters the forward bandwidth specifications:

It should be noted that when Auto-Detect AVP is set to YES,the MAGIC server shall
detect traffic bandwidth in both the forward and return directions and make
appropriate assignments.In this scenario,Auto-Detect will
require two options.This
specification indicates symmetric versus asymmetric auto detection by providing the
additional enumeration in Auto-Detect
When both Auto-Detect are set to YES and a Requested-Bandwidth is provided,
Auto-Detect is ignored,and the request

is rejected with the reason code

ERROR_CODE_ILLEGAL_PARAMETER.

1.1.2.1 Communication Parameter Related AVP Groups

1.1.2.1.1 Communication-Request-Parameters AVP-Group

The Communication-Request-Parameters AVP Group is used to exchange the
parameter necessary to establish,change,or
group is only sent from the clients to MAGIC to request initial throughputs or to re-
negotiate throughput after an already granted request (i.e.,request more or less
throughput,higher or

release the communication.This AVP

lower priorities,etc.).

The format of a Communication-Request-Parameters AVP-Group is shown below:

ARINC SPECIFICATION 839-Page 82

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

<Communication-Request-Parameters>::=< Diameter AVP:20001>

[Accounting-Enabled]

{Profile-Name}
optional[Requested-Bandwidth]
optional[Requested-Return-Bandwidth]
[Required-Bandwidth]
optional
[Required-Return-Bandwidth]
optional
optional
[Priority-Type]
optional
optional [Priority-Class]
optional [DLM-Name]
optional [QoS-Level]
optional [Flight-Phase]
optional [Altitude]
optional [Airport]
optional [TFTtoGround-List]
optional [TFTtoAircraft-List]
optional [NAPT-List]
optional
optional
optional[Timeout]

[Keep-Request]
[Auto-Detect]

*[AVP]

1.1.2.1.2 Communication-Answer-Parameters AVP-Group

The Communication-Answer-Parameters
parameter established,changed,or released by MAGIC.This AVP group is only
sent

from MAGIC to the clients to inform them about

the granted throughputs after a

AVP-Group is

exchange the

used to

request or

to inform a client about

changes of previously granted throughput values.

The format of a Communication-Answer-Parameters AVP-Group

is

shown

below:

<Communication-Answer-Parameters>:=<Diameter AVP:20002>

{Profile-Name}
{Granted-Bandwidth}
{Granted-Return-Bandwidth }
{Priority-Type}
{Priority-Class}
{TFTtoGround-List}
{TFTtoAircraft-List}
{QoS-Level}
{Accounting-Enabled}
{DLM-Availability-List }
{Keep-Request}
{Auto-Detect}
{Timeout}

optional [Flight-Phase]
optional [Altitude]
optional [Airport]
optional [NAPT-List]
optional

[Gateway-IPAddress]

*[AVP]

1.1.2.1.3 Communication-Report-Parameters AVP-Group

The Communication-Report-Parameters AVP Group is used to exchange the
parameters that have been changed by MAGIC or
MAGIC.This AVP group is only sent
changes.

from MAGIC to the clients to inform them of

the link controllers connected to

the

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

ARINC SPECIFICATION 839-Page 83

MAGIC shall only put
Group that have been changed.

the AVPs

into the Communication-Report-Parameters AVP-

The format of a Communication-Report-Parameters AVP-Group

is

shown below:

<Communication-Report-Parameters>::=<Diameter AVP:20003>

{Profile-Name}

optional [Granted-Bandwidth]
optional [Granted-Return-Bandwidth]
optional [Priority-Type]
optional [Priority-Class]
optional [TFTtoGround-List]
optional[TFTtoAircraft-List]
optional [QoS-Level]
optional [DLM-Availability-List]
optional [NAPT-List]
optional [Gateway-IPAddress]

*[AVP]

1.1.2.2 Traffic Flow Related AVP Groups

1.1.2.2.1 TFTtoGround-List AVP Group

The TFTtoGround-List AVP-Group can contain between 1 and 255 TFTtoGround-
this group is defined as follows:
Rule AVPs.The format of

<TFTtoGround-List>::=<Diameter AVP:20004>

1*255

{TFTtoGround-Rule}

1.1.2.2.2 TFTtoAircraft-List AVP Group

The TFTtoAircraft-List AVP-Group can contain between 1 and 255 TFTtoAircraft-
Rule AVPs.The format of

this group is defined as follows:

<TFTtoAircraft-List>::=<Diameter AVP:20005>

1*255

{TFTtoAircraft-Rule}

1.1.2.2.3 NAPT-List AVP Group

The AVP NAPT-List AVP-Group can contain between 1 and 255 NAPT-Rule AVPs.
The format of

this group is defined as follows:

<NAPT-List>::=<Diameter AVP:20006>
{NAPT-Rule}

1*255

1.1.2.3 Data Link Module Related AVP Groups

1.1.2.3.1

DLM-List AVP-Group

The DLM-List AVP-Group contains a DLM-Info ACP per
List AVP-Group is defined as follows:

link controller.The DLM-

ARINC SPECIFICATION 839-Page 84

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

<DLM-List>::=<AVP Header:20007>

1*{DLM-Info}

1.1.2.3.2

DLM-Info

AVP-Group

The DLM-Info AVP group shows a list values characterizing a DLM.The DLM-Info
AVP-Group is defined as follows:

<DLM-Info>::=<AVP Header:20008>
{DLM-Name}
{DLM-Available}
{DLM-Max-Links}
{DLM-Max-Bandwidth}
optional [DLM-Max-Return-Bandwidth]
{DLM-Allocated-Links}
{DLM-Allocated-Bandwidth }
optional [DLM-Allocated-Return-Bandwidth]

{DLM-QoS-Level-List}
optional [DLM-Link-Status-List]

*[AVP]

1.1.2.3.3 DLM-QoS-Level-List AVP-Group

The DLM-QoS-Level-List AVP-Group can contain up to three QoS-Level AVP
entries.It defines which QoS characteristics the DLM is capable to support.The
AVP-Group is defined as follows:

<DLM-QoS-Level-List>::=<AVP Header:20009>

1*3{QoS-Level}

1.1.2.3.4 DLM-Link-Status-List AVP-Group

The DLM-Link-Status-List AVP-Group is
AVPs.Each contained AVP is representing one data-link type that can be supported
by the DLM.The DLM-Link-Status-List AVP-Group

list of Link-Status-Group

is defined as follows:

containing a

<DLM-Link-Status-List>::=< AVP Header:20010>

1*{Link-Status-Group}

1.1.2.3.5

Link-Status-Group

AVP-Group

For each available link in the link controllers listed in the Data-Link-Status-List AVP-
Group,a Link-Status-Group AVP group shall

be included.

The Link-Status-Group AVP-group is defined as

follows:

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

ARINC SPECIFICATION 839-Page 85

<Link-Status-Group>::=<AVP Header:20011>

{Link-Name}
{Link-Number}
{Link-Available}
{QoS-Level}
{Link-Connection-Status}
{Link-Login-Status}
{Link-Max-Bandwidth}
optional {Link-Max-Return-Bandwidth}
[Link-Alloc-Bandwidth]
optional[Link-Alloc-Return-Bandwidth]
optional [Link-Error-String}

*[AVP]

1.1.2.4 Accounting Related AVP Groups

1.1.2.4.1 CDRs-Active AVP-Group

The CDRs-Active AVP-Group contains all active CDR identifiers(CDR is
active).

currently

<CDRs-Active>::=<Diameter AVP:20012>

1*

{CDR-Info}

this group is included into an answer,it must

If
Group.Each active Call Data Record is represented with one single entry.

include at

least one CDR-Info AVP-

1.1.2.4.2

CDRs-Finished

AVP-Group

The CDRs-Finished AVP-Group contains all CDR identifiers(CDR has
and is no longer active).

been closed

<CDRs-Finished>::=<Diameter AVP:20013>

1*

{CDR-Info}

this group is included into an answer,it must

If
Group.Each finished Call Data Record that
represented with one single entry.

include at

least one CDR-Info AVP-

is known in the MAGIC system is

1.1.2.4.3

CDRs-Forwarded

AVP-Group

The CDRs-Forwarded AVP-Group contains all CDR identifiers(CDR
closed and is no longer active).Note:CDRs of sessions which have been
terminated,are first
they are returned as "forwarded."

"finished"when

returned as

read.After

(second

that

has been

reading),

<CDRs-Forwarded>::=<Diameter AVP:20014>

1*

{CDR-Info}

If this group is included into an answer,it must
Each forwarded Call Data Record is represented with one single entry.

include at

least one CDR-Info AVP.

ARINC SPECIFICATION 839-Page 86

ATTACHMENT 1
CLIENTINTERFACE PROTOCOL DETAILS

1.1.2.4.4 CDRs-Unknown AVP-Group

The CDRs-Unknown AVP-Group contains all CDR identifiers that directly have been
requested by the client,but

that are not known in the system.

<CDRs-Forwarded>::=<Diameter AVP:20015>

1*

{CDR-ID}

this group is included into an answer,it must

If
Each unknown Call Data Record identifier is represented with one single entry.

least one CDR-ID AVP.

include at

1.1.2.4.5 CDRs-Updated AVP-Group

The CDR-Updated AVP-Group contains a list accounting record identifiers that have
been closed and started due to a MAGIC-Accounting-Control-Request(MACR)
message.The CRD-ID AVP-Group is defined as follows:

<CDRs-Updated>::=<Diameter AVP:20016>
{CDR-Start-Stop-Pair}

1*

1.1.2.4.6 CDR-Info AVP-Group

The CDR-Info AVP-Group contains an identifier of an accounting record and
optionally the content of

that record.The CDR-Info AVP-Group is defined as follows:

<CDR-Info>::=<Diameter AVP:20017>

1*

{CDR-ID}

1.1.2.4.7 CDR-Start-Stop-Pair AVP-Group

The CDR-Start-Stop-Pair AVP-Group contains two accounting record identifiers.It
shows which accounting record has been stopped due to a MAGIC-Accounting-
Control-Request
(MACR).The CDR-Started AVP contains the accounting record
identifier of
as follows:

the succeeding record.The CDR-Start-Stop-Pair AVP-Group is defined

<CDR-Start-Stop-Pair>::=<Diameter AVP:20018>

{CDR-Stoped}
{CDR-Started}

1.1.2.5 Authentication Related AVP Groups

1.1.2.5.1 Client-Credentials AVP-Group

A client can authenticate itself by using certificates or it can send its credentials in
the MCAR.The Client-Credentials AVP-Group can be used to send the user-

credentials to the server.

The Client-Credentials AVP-Group is defined as follows:

ARINC SPECIFICATION 839-Page 87

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

<Client-Credentials>::=<AVP Header:20019>

{User-Name}
{Client-Password}

1.2 Data Formats

1.2.1 NAPT String

Clients with special communication requirements may need a NATed or NAPTed
environment to be reachable by a ground monitoring system or to be integrated in a
wider infrastructure spread between aircraft and ground.

1.2.1.1 NAPT String Definition

The NAPT string is following the following syntax:

<NAT-Type>,<Source-IP>,<Destination-IP>,<IP-Protocol>,<Destination-Port>,<Source-
Port>,<to-IP[IP-range]>,<to-Port

[Port-range]>

The table below provides detail for each field that a NAPT string can contain:

Field

<NAT-Type>

<Source-IP>

<Destination-IP>

Description
This field contains a fixed string comprising the two following
values to differentiate between a source translation and a
destination translation:
SNAT:The source IP address and/or port is translated
DNAT:The destination IP address and/or port is translated

The Network Address Translation(NAT)engine selects
packets to be NATed if their source IP is matching this field.
This field may be empty,in which case all source IPs are
matching.

If this field is not empty,it comprises two subfields containing
an IPv4 address and a network mask in the typical IPv4
"dotted"notation.The two subfields are separated by a "dot."
The format of this field is:<IPv4-Address>.<IPv4-Netmask>.

The single IP address 147.23.45.9 matches the following
example:
147.23.45.9.255.255.255.255
A complete 192.168.20.0/24 subnetwork matches the
following example:
192.168.20.0.255.255.255.0

The NAT engine selects packets to be NATed if their
destination IP is matching this field.This field may be empty,
in which case all source IPs are matching.

If this field is not empty,it comprises two subfields containing
an IPv4 address and a network mask in the typical IPv4
"dotted"notation.The two subfields are separated by a "dot."
The format of this field is:<IPv4-Address>.<IPv4-Netmask>.

The single IP address 147.23.45.9 matches the following
example:
147.23.45.9.255.255.255.255

ARINC SPECIFICATION 839-Page 88

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

Field

<IP-Protocol>

<Destination-Port>

<Source-Port>

<to-IP

[IP-range]>

Description
A complete 192.168.20.0/24 subnetwork matches the
following example:
192.168.20.0.255.255.255.0
This field is an 8-bit integer protocol number of packets that is
submitted to the NAT rule.The values are based on the IANA
Assigned Internet Protocol Numbers(i.e.,6=TCP,17=
UDP).

This field is used to define a single destination port or a
destination port range that needs to be NATed by the NAT
engine.This field may be empty,in which case all source
ports are matching.
If this field is not empty it can be a single value represented
by a 16 bit integer value or a range represented by two 16 bit
integer values separated by a "dot."

If standard HTTP traffic needs to be NATed the following
value would match:
80
If a port range addressing the well-known ports needs to be
NATed the following string would match:
0.1023

This field is used to define a single source port or a source
port range that needs to be NATed by the NAT engine.This
field may be empty,in which case all source ports are
matching.
If this field is not empty it can be a single value represented
by a 16 bit integer value or a range represented by two 16 bit
integer values separated by a "dot."

If standard SNMP-Messages needs to be NATed the
following value would match:
161
If a port range other than the well-known ports needs to be
NATed the following string would match:
1024.65535

This field is defining the IP(s)for translation and translates a
source or a destination IP address based on the<NAT-
Type>.It can be a single “dot separated ”IPv4 address or a
range represented by two “dot separated ”IPv4 addresses
that are concatenated by a "dot."
For Static Network Address Transfer(SNAT),the source IP
address of packets is translated to the IP address(es)in a
round-robin fashion.
For Dynamic Network Address Translation(DNAT),the

destination IP address of packets is translated to the IP
address(es)in a round-robin fashion.

A company that has negotiated a complete IP range from
147.23.45.9 to 147.23.45.71 with its service provider on its
gateway to the internet for example uses the following value
to translate the private corporate network components into
public routable addresses:
147.23.45.9.147.23.45.71

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

Field

Description

ARINC SPECIFICATION 839-Page 89

<to-Port
range]>

[Port-

A single gateway address as usually used in a link controller
environment (such as a home DSL router or a Satcom)
usually only has one IP address configured.A valid entry for
a single entry is shown in the following example:
147.23.45.9

This field is only used in case the<IP-Protocol>is set to
TCP or UDP.
In this case,the field is defining the port(s)for translation and
translates a source or a destination port based on the<NAT-
Type>.It can be a single 16 bit integer value or a range
represented by two 16 bit integer values concatenated by a
"dot."
For SNAT,the source port of packets is translated to this
port(s)in a round-robin fashion.
For DNAT,the destination port of packets is translated to this
port(s)in a round-robin fashion.

If both values are equal or the second value is containing a
"0,"only a single port is used for translation.This would be
the same behavior as if the second value is not there.

The following three examples show how to forward to a
single http server port:
80
80.0
80.80

The following example shows how to forward to a port range
between 2000 and 2099:
2000.2099

If this value is not specified,the port is:
Dynamically altered if the<NAT-Type>is set to DNAT.
Not altered-when possible-supports dynamic NAT/
masquerading mechanisms if the<NAT-Type>is set to
SNAT.

1.2.1.2 NAPT String Behavior

A service provider usually assigns IP addresses to aircraft links dynamically.The
mechanism to do this is Dynamic Host Control Protocol(DHCP).If the service
provider has foreseen mechanisms to not swap IPs between aircrafts (i.e.,based on
the aircraft registration number),this causes a situation where an aircraft may not
get the same IP address as it already received for the last connection.

A NAPT string used to forward traffic from the ground to dedicated software hosted

on a unit that can be addressed within the local LAN of an aircraft must include the
service provider assigned IP address of the aircraft since this is the only address

that can be seen from the internet.

A service provider has,for example,an IP network 145.230.22.0/24 to provide
clients with internet connectivity.An aircraft using this Network could get one of the
253 addresses assigned.Under the assumption that the provider has implemented

ARINC SPECIFICATION 839-Page 90

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

very good mechanisms to provide an aircraft always with the same IP(i.e.,
145.230.22.194),a simple NAT rule could be set up to reach a local web server at
port 80 on IP address 192.168.16.1.

DNAT,, 145.230.22.194,6,80,,192.168.16.1,80

This simple NAT rule would allow any source from the internet to reach the web
server located in the aircrafts local network.But this simple NAT rule does not work
in case the service provider does not
assignment.If the normal DHCP behavior is set up,the aircraft could get any
address out of the complete IP range(i.e.,in this case 145.230.22.2 to
145.230.22.254)under the assumption that the gateway IP address of the service
provider itself is configured to be 145.230.22.1.In this case,the rule would have to
be adapted to cover all possibilities.The new rule would look as follows:

implement a "some-how"static DHCP

DNAT,, 145.230.22.0.255.255.255.0,6,80,, 192.168.16.1,80

If the service provider is running out of IP addresses,a decision could be made to
extend the pool of IP addresses by another 145.230.23/24 network to double the
the aircraft configuration willbe updated as well,because
range.This implies that
with the previously given example the new addresses 145.230.23.0 to
145.230.23.254 would not match.In order to avoid a frequent update of the network
address ranges,MAGIC needs to refer to the assigned IP Address by using a
variable
fields or those fields are empty,MAGIC needs to fill the assigned IP address into the
NAPT string,before applying to the NAPT engine.The NAPT strings matching to
the example would look as follows:

this string is present

format 号 LinkIp%.

in one of

the IP

name

the

in

If

DNAT,,LinkIp%,6,80,, 192.168.16.1,80
DNAT,,,6,80,,192.168.16.1,80

1.2.1.3 NAPT String Requirements

The MAGIC NAPT engine shallbe able to handle extended NAPT rule for both type
DNAT and SNAT to support complete service provider address-spaces in all
Fields.

IP-

NAPT

engine

The
MAGIC
within all
provider has assigned to the gateway responsible for the matching flows.

process
IP-Fields of a NAPT string and exchange it with the address the service

a“若号LinkIP 号 ”placeholder

shallbe

able

to

The MAGIC NAPT engine shall be able to process empty IP-Fields of a NAPT string
and exchange it with the address the service provider has assigned to the gateway
responsible for the matching flows.

The MAGIC customization and configuration fields shall be able to process address
ranges within NAPT

IP fields.

string

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

ARINC SPECIFICATION 839-Page 91

1.2.2

Traffic Flows and White Lists

In contrast to the Diameter protocol,a session in MAGIC may consist of more than
one connection.Therefore,the payload of a single session can be all packages sent
from a range of source IP/ports to a range of destination IP/ports.
In order to keep the configuration efforts as low as possible,MAGIC is containing
information in the profile of the clients.This information can be considered as a
white list of addresses/ports a client is allowed to communicate to.For both client
types(static clients and dynamic clients),the white list
allowed ranges.

is defining the maximum

1.2.2.1 Traffic Flows Template Format

The mechanism that shall be used for this kind of information is the Traffic Flow
Template(TFT).

The TFT describes these IP and port ranges and thus specifies which MAGIC
session is associated with a given packet. Up to 2 5 5 TFTs can be used to specify
the packets belonging to a MAGIC session.There is at least one TFT for each
session.

The string format for TFTs from Aircraft-to-Ground is as follows:

filter

identifier>,< evaluation precedence index>[,< source address

_iTFT=[< cid>,[< packet
and subnet mask>[,< destination address and subnet mask>[,< protocol number(ipv4)/ next
header(ipv6)>[ ,< destination port
index(spi)>[,<type of service(tos)(ipv4)and mask /traffic
(ipv6)>/I]

range>[< ipsec security parameter

range>[,< source port

class(ipv6)and mask>[,<flow label

The string format for the TFTs from Ground-to-Aircraft is as follows:

filter

identifier>,< evaluation precedence index>[,< source address

+CGTFT=[< cid>,[< packet
and subnet mask>[,<destination address and subnet mask>[<protocol number(ipv4)/next
header( ipv6)> [ ,< destination port
index(spi)>[,<type of service (tos)(ipv4)and mask /traffic class(ipv6)and mask>[,<flow label
(ipv6)>7

range>[ < ipsec security parameter

range>[, < source port

]

Both TFTs are following the string format as defined in 3 GPP TS 23 .0 6 0 and contain
various types of information (such as source IP address ranges,destination IP
ranges,etc.).The detailed
address ranges,source port
description of TFTs and of their format can be found in 3GPP 23.060 V10.1.0,
Section 15.3.

ranges,destination port

COMMENTARY

It is assumed that the according return path is assigned by the
network,if a TFT for one direction is defined.This means that only
one TFT is necessary stream.The TFT should be defined in
accordance to its main direction.

ARINC SPECIFICATION 839-Page 92

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

1.2.2.2 White List Validation

MAGIC shall compare TFT strings provided by a client via the Client
Handler with predefined white lists on a content basis by interpreting each value
against the according profile entries.

Interface

MAGIC shallgrant the throughput as long as the TFT value ranges are inside the
client's white list ranges,if it was able to open a pipe on a communication system
(such as a Satcom)

MAGIC shall reject the request with an error message in its answer if one of the
values provided in the TFTs is outside the predefined white list range.

Client hosted on a server with the IP address 192.168.0.3 belonging to the network
192.168.0.0/24 that is managing the internet access of the passengers in the
172.16.0.0/16 network is,for example,per white list entries allowed to communicate
to the destination subnetworks with the following IP/Port ranges.The entries are
defined as follows:

·

·

192.168.0.3/24 to any well-known TCP port (0...1023)within the network

10.10.0.0/24
172.16.0.0/17 on allTCP/UDP ports in the public internet

The following TFTs would be part of the client's profile:

_ITFT=.192.168.0.0.255.255.255.0, 10.16.0.0.255.255.0.0,6,0.1023,0.65535,,,
_ITFT=..172.16.0.0.255.255.128.0,0.0.0.0.0.0.0.0,6,0.65535,0.65535,.,
_ITFT=,..172.16.0.0.255.255.128.0,0.0.0.0.0.0.0.0, 17,0.65535,0.65535,,,

The following paragraphs provide some examples for correct,but also for failed
white list validations against destination addresses and ports.The same type of
rules apply to all white list entries(such as priority range,etc.).

MAGIC would open a pipe for communication to ground with the following request
based on the above mentioned profile entries:

● TCP traffic from 192.168.0.3/32 to 10.16.0.0/32:80

·
·

·

TCP traffic from 192.168.0.7/32 to 10.16.0.16/32:21
TCP traffic from multiple IPs in the range of 172.16.64.0/17 to 0.0.0.0/0:Any.

UDP traffic from multiple IPs in the range of 172.16.64.0/17 to 0.0.0.0/0:Any.

MAGIC would not open a pipe for communication to ground with the following

request based on the above mentioned profile entries:

·
·

·

UDP traffic from 192.168.0.3/32 to 10.16.0.0/32:80
TCP traffic from multiple IPs in the range between 172.16.127.1 and
172.16.129.255 on port 80 (because the higher IP-range value is not
mapping)

UDP traffic from 192.168.0.10/32 to 10.18.15.13 on Ports O to 2047
(because the IP address and the higher port range value is not mapping)

COMMENTARY

The aim of the examples is to show that a "normal"sting match
validation is not sufficient enough to grant or to reject a client request.

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

ARINC SPECIFICATION 839-Page 93

1.2.3 Control Data Records(CDRs)

MAGIC shall track all off-board communication and save associate accounting data

in its internal database.

Such accounting data in of Call Data Records(CRDs)are reported on request over
the Diameter interface to clients with appropriate user rights.

1.2.3.1 CDR Format

The format of the Diameter CDRs consists of one string that contains a comma
separated list of parameters in the following format:

CDR:=Parameter[,Parameter]*

Each parameter comprises a Marker(defining the name of the parameter)and a
value.The Marker and the value are also separated with a comma.

Parameter

::=Marker,Value

The Markers are pre-defined and each of them has a dedicated type.The following

Markers are defined:

Marker
ID
SESSION
NAME

STARTDATE

Value
Integer
String
String
String

STARTTIME

String

STOPDATE

String

STOPTIME

String

TXBYTE

RXBYTE
STATUS

Long

Long
StatusClass

Comment
Unique identification number of the CDR.
Diameter Session ID that is tracked by this CDR.
Name of the client that is running the session.
Start date of the session in the format:“YYYY ”-
"MM"-"DD" (where YYYY is the year,MM is the
month,and DD is the day).
UTC start time of the session in the format:
hh:mm:ss(where hh is the hour,mm is the minute,
and ss is the second).
Stop date of the session in the format:YYYY-MM-
DD( where YYYY is the year, MM is the month, and
DD is the day).

(to ground)
(from ground)

End time of the session in the format:hh:mm:ss
(where hh is the hour,mm is the minute,and ss is
the
second)
Bytes transmitted
Bytes received
Status of this CDR.The enumeration can take the
following values:
NO_STATUS
ACTIVE
FINISHED
FORWARDED

DLMNAME

String

The StatusClass is described below.
Name of datalink.

ARINC SPECIFICATION 839-Page 94

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

Marker
QOSCLASS

Value
QoSClass

ENDREASON

EndReasonclass

FLIGHTNUMBER

TAILNUMBER

DURATION

ENTRYDATE

String

String

Long

String

Comment
The Quality of Service class that has been applied
to the session.The enumeration can take the
following
NO_ QoS
BestEffort
AssuredForwarding
ExpeditedForwarding

values:

The QosClass is described below.
The reason why a session has been shut down.
The enumeration can take the following values:
NO_REASON
ClientTermination
ServerTermination
AssignmentTermination
Error
CLIENT_REQUEST
FLIGHT_PHASE_CHANGED

SESSION_DIED_DURING_OPERATION
SHUTDOWN

The EndReasonClass is described below.

Flight number of the flight the communication
session is belonging to.
Tail-ID of the aircraft that established the
communication
The duration of the session in seconds.

session.

Date and time of last change to CDR in the format:
"YYYY”-“MM”-"DD"T“hh":"mm” :"ss" (where YYYY is
the year,MM is the month,DD is the day,hh is the
hour,mm is the minute,and ss is the second).

Value fields may be empty if the value is not admitted to accounting
recording by configuration.

COMMENTARY

1.2.3.2 StatusClass Format

The Status class enumeration type is defined as follows:

StatusClass::=ENUM{
NO_STATUS=0,
ACTIVE= 1,
FINISHED=2,
FORWARDED= 3

}

1.2.3.3 QoSClass Format

The QoS class enumeration type is defined as follows:

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

ARINC SPECIFICATION 839-Page 95

QoSClass

::=ENUM{

NO_QOS=0,
BestEffort = 1,
AssuredForwarding=2,
ExpeditedForwarding

=3

}

1.2.3.4 EndReasonClass Format

The Release Reason class enumeration type is defined as follows:

EndReasonClass

::=ENUM{

NO_REASON=0,
= 1,
ClientTermination
ServerTermination
=2,
AssignmentTermination
Error=4,
CLIENT_ REQUEST= 5 ,
FLIGHT_ PHASE_ CHANGED= 6 ,

=3,

SESSION_DIED_DURING_OPERATION=7,
SHUTDOWN= 8

}

1.3 Result-and Message-Code Formats

1.3.1 RESULT-CODE

The following table contains valid values that can be in the RESULT-CODE AVP.
This AVP is part of the answer commands as defined in [DBP-RFC].
The following classes of errors are identified by the thousands digit in the decimal

notation:

·

·

·

·

·

1XXX(Informational)

2XXX (Success)

3XXX(Protocol Errors)

4XXX(Transient Failures)

5XXX(Permanent Failure)

ARINC SPECIFICATION 839- Page 96

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

Name
DIAMETER_MULTI_ROUND_AUTH
DIAMETER_SUCCESS
DIAMETER_LIMITED_SUCCESS
DIAMETER_COMMAND_UNSUPPORTED
DIAMETER_UNABLE_TO_DELIVER
DIAMETER_REALM_NOT_SERVED
DIAMETER TOO BUSY
DIAMETER_LOOP_DETECTED
DIAMETER REDIRECT INDICATION
DIAMETER_CLIENT_UNSUPPORTED
DIAMETER INVALID HDR BITS
DIAMETER INVALID AVP BITS
DIAMETER UNKNOWN PEER
DIAMETER AUTHENTICATION REJECTED
DIAMETER OUT OF SPACE
ELECTION LOST
DAMETER AVP UNSUPPORTED
DIAMETER UNKNOWN SESSION ID
DIAMETER AUTHORIZATION REJECTED
DIAMETER INVALID AVP VALUE
DIAMETER MISSING AVP
DIAMETER RESOURCES EXCEEDED
DIAMETER CONTRADICTING AVPS
DIAMETER AVP NOT ALLOWED
DIAMETER AVP OCCURS TOO MANY TIMES
DIAMETER NO COMMON CLIENT
DIAMETER UNSUPPORTED VERSION
DIAMETER UNABLE TO COMPLY
DIAMETER INVALID BIT IN HEADER
DIAMETER INVALID AVP LENGTH
DIAMETER INVALID MESSAGE LENGTH
DIAMETER INVALID AVP BIT COMBO
DIAMETER_NO_COMMON_SECURITY

Value
1001
2001
2002
3001
3002
3003
3004
3005
3006
3007
3008
3009
3010
4001
4002
4003
5001
5002
5003
5004
5005
5006
5007
5008
5009
5010
5011
5012
5013
5014
5000
5016
5017

The Result-Code AVP is per [DBP-RFC]subject to be extended with additional
codes,but for MAGIC related Error Codes a new AVP has been defined to capture
these issues.The MAGIC-Status-Code AVP is handling such values.

1.3.2 MAGIC-Status-Code Values

The following is a list of error and info codes that can be set in the MAGIC-Status-

Code AVP.

Name
MAGIC-ERROR MISSING-AVP
MAGIC-ERROR_AUTHENTICATION-FAILED,
MAGIC-ERROR_UNKNOWN-SESSION
MAGIC-ERROR_MAGIC-NOT-RUNNING
MAGIC-ERROR ENUMERATION-OUT-OF-RANGE,
MAGIC-ERROR_MALFORMED-LINK-STRING
MAGIC-ERROR_MALFORMED-QOS-STRING
MAGIC-ERROR_MALFORMED-TFT-STRING

Value
1000
1001
1002
1003
1004
1005
1006
1007

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

ARINC SPECIFICATION 839-Page 97

MALFORMED-NAPT-STRING
MALFORMED-AIRPORTLIST-STRING

CLIENT-UNREGISTRATION,
ILLEGAL-FLIGHT-PHASE
ILLEGAL-PARAMETER
STATUS-ACCESS-DENIED
ACCOUNTING-ACCESS-DENIED,
ACCOUNTING-CONTROL-DENIED
MAGIC-SHUTDOWN
ACCOUNTING-INVALID-CDRID,
MALFORMED-CDR-STRING

Name
MAGIC-ERROR_MALFORMED-DATA-LINK-STRING,
MAGIC-ERROR_MALFORMED-CHANNEL-STATUS-STRING
MAGIC-ERROR_ADD-SESSION-FAILED
MAGIC-ERROR_MODIFY-SESSION-FAILED
MAGIC-ERROR_OPEN-LINK-FAILED-DEPRECTED
MAGIC-ERROR_NO-ENTRY-IN-BANDWIDTHTABLE,
MAGIC-ERROR NO-OPEN-LINKS
MAGIC-ERROR_ _AMBIGOUS-BANDWIDTHTABLE-PERCENTAGE
MAGIC-ERROR NO-FREE-BANDWIDTH
MAGIC-ERROR MISSING-PARAMETER
MAGIC-ERROR_CLIENT-REGISTRATION,
MAGIC-ERROR
MAGIC-ERROR
MAGIC-ERROR
MAGIC-ERROR
MAGIC-ERROR
MAGIC-ERROR
MAGIC-ERROR
MAGIC-ERROR
MAGIC-ERROR
MAGIC-ERROR SESSION-IS-BLOCKED-DEPRECTED
MAGIC-ERROR
MAGIC-ERROR
MAGIC-ERROR MALFORMED-FLIGHTPHASELIST-STRING
MAGIC-ERROR MALFORMED-ALTITUDELIST-STRING
MAGIC-ERRORI MALFORMED-DLMLIST-STRING
MAGIC-ERROR MALFORMED-PRIO-CLASS-STRING
MAGIC-ERROR PROFILE-DOES-NOT-EXITS
MAGIC-ERROR TFT-INVALID,
MAGIC-ERROR NAPT-INVALID
MAGIC-INFO SET-LINK-QOS
MAGIC-INFO REMOVE-LINK-QOS
MAGIC-INFO SET-DEFAULTROUTE
MAGIC-INFO REMOVE-DEFAULTROUTE
MAGIC-INFO SET-POLICYROUTING
MAGIC-INFO REMOVE-POLICYROUTING
MAGIC-INFO SET-NAPT
MAGIC-INFO REMOVE-NAPT
MAGIC-INFO SET-SESSION-QOS
MAGIC-INFO REMOVE-SESSION-QOS
MAGIC-ERROR INTERNAL-ERROR
MAGIC-ERROR AIS-INTERRUPED
MAGIC-ERROR
MAGIC-ERROR AIRPORT-ERROR
MAGIC-ERROR ALTITUDE-ERROR
MAGIC-ERROR NO-BANDWIDTH-ASSIGNMENT
MAGIC-ERROR REQUEST-NOT-PROCESSED
MAGIC-ERROR OPEN-LINK-FAILED
MAGIC-ERROR_LINK-ERROR

FLIGHT-PHASE-ERROR

Value
1008
1009
1010
1011
1012
1013
1014
1015
1016
1017
1018
1019
1020
1021
1022
1023
1024
1025
1026
1027
1028
1029
1030
1031
1032
1033
1034
1035
1036
1037
1038
1039
1040
1041
1042
1043
1044
1045
1046
1047
1048
2000
2001
2002
2003
2004
2005
2006
2007

ARINC SPECIFICATION 839-Page 98

ATTACHMENT 1
CLIENT INTERFACE PROTOCOL DETAILS

Name
MAGIC-ERROR CLOSE-LINK-FAILED
MAGIC-ERROR MAGIC-FAILURE
MAGIC-INFO FORCED REROUTING
MAGIC-ERROR UNKNOWN-ISSUE
MAGIC- ERRORAVIONICSDATA- MISSING

Value
2008
2009
2010
3000
30 0 1

The MAGIC server has with the MAGIC-Status-Code AVP the capability to inform a
client about actual events that could occur.For this purpose,MAGIC is using the
Reports to inform a client.

The MAGIC server has with the MAGIC-Status-Code AVP the capability to inform
the client about MAGIC specific errors in a request.For this purpose,MAGIC is
including this AVP into the answer back to a client.

ARINC SPECIFICATION 839-Page 99

ATTACHMENT 2
MODIFICATIONS TO IEEE 802.21-2008

ATTACHMENT 2 MODIFICATIONS TOIEEE 802.21-2008

2.1 NEW PRIMITIVE FOR MIH_SAP-MIH_LINK_RESOURCE

This new primitive is used by an MIH User to request or release resources on a link.

2.1.1 MIH_LINK_RESOURCE.Request

NAME
DestinationIdentifier

Data type

MIHF_ID

LinkIdentifier

LINK_ TUPLE_ ID

ResourceAction

RESOURCE_AC_TYPE

BearerIdentifier

BEARER_ID

QoSParameters

QOS_PARAM

Description
This identifies the local MIHF or a
remote MIHF that is the destination
of this request.

Identifier of the link for resource
request.For local event
subscription,Point of Access(PoA)
link address need not be present if
the link type lacks such a value.

Indicates whether a resource
request or release should be
performed.
(Optional)If additional resources
should be allocated for an already
existing bearer,this parameter
identifies

this bearer.

( Optional) If additional resources
should be allocated,this parameter
defines the required resources.

For a request of resources,the parameter QoSParameters is always required.If the
additional resources for an already existing bearer should be allocated,the
Bearerldentifier is also required.

For a release of resources,the Bearerldentifier is always required.

2.1.2 MIH_LINK_RESOURCE.Confirm

This primitive is issued by an MIHF to report the result of a MIH_ Link_ Resource
request.

NAME

SourceIdentifier

Data type
MIHF_ID

LinkIdentifier

Status

BearerIdentifier

LINK_ TUPLE_ ID

STATUS
BEARER_ID

Description
This identifies the invoker of this
primitive,which can be either the local
MIHF or a remote MIHF.
Identifier of the link for which resource
request or release was performed.
Status of operation.

( Optional) If additional resources have
been successfully allocated,this
parameter identifies the bearer
associated with the new resources.

2.2 NEW PRIMITIVE FOR MIH_LINK_SAP-LINK_RESOURCE

This primitive is used by the MIHF to request or release resources on a link-layer

connection.

ARINC SPECIFICATION 839-Page 100

ATTACHMENT 2
MODIFICATIONS TO IEEE 802.21.2008

2.2.1

LINK_RESOURCE.Request

NAME

Data type

ResourceAction

RESOURCE_AC_TYPE

BearerIdentifier

BEARER_ID

QoSParameters

QOS_PARAM

Description
Indicates whether a resource request or
release should be performed.
(Optional)If additional resources should
be allocated for an already existing
bearer,this parameter identifies this
bearer.
(Optional)If additional resources should
be allocated,this parameter defines the
required

resources.

For a request of resources,the parameter QoSParameters is always required.If the
additional resources for an already existing bearer should be allocated,the
Bearerldentifier is also required.

For a release of resources,the Bearerldentifier is always required.

2.2.2 MIH_LINK_RESOURCE.Confirm

This primitive is issued by an MIHF to report the result of a MIH_ Link_ Resource
request.

NAME
Status

BearerIdentifier

Data type
STATUS
BEARER_ID

Description
Status of operation.

If additional resources have been
successfully allocated,this parameter
identifies the bearer associated with the
new

resources.

2.3 NEW DATA TYPES

These are new data types,not included in IEEE 802.21-2008.

Data Type Name
BEARER_ID

QOS_PARAM

Derived from
UNSIGNED_INT(1)

SEQUENCE(
ID,
COS

LINK_DATA_RATE_FL,

LINK_ DATA_ RATE_ RL
)

HARDWARE_HEALTH

OCTET_STRING

LINK_ DATA_RATE_ FL

UNSIGNED_INT(4)

LINK_ DATA_RATE_ RL

UNSIGNED_INT(4)

Definition
Supports up to 256 different bearers.

Specifies the Class of Service( CoS)
as well as the bandwidth requested for
forward and return link individually.

A non- NULL terminated string with a
maximum length of 253 octets,
representing information on hardware
health in displayable text.
Represents maximum data rate in
kbps in forward link.Type equivalent
to IEEE 802.21 LINK_DATA_RATE
data
Represents maximum data rate in
kbps in return link.Type equivalent to
IEEE 802.21 LINK_DATA_RATE data
type.

type.

ARINC SPECIFICATION 839-Page 101

ATTACHMENT 2
MODIFICATIONS TO IEEE 802.21-2008

2.4 MODIFIED DATA TYPES

These data types have been modified when compared to IEEE 802.21-2008.

Data Type Name

DEV_STATES_REQ

Derived from

BITMAP(16)

LINK_PARAM_TYPE

CHOICE(

Definition
A list of device status request.
Bitmap Values:
Bit 0:DEVICE_INFO
1 :BATT_LEVEL
Bit
2:HARDWARE_HEALTH
Bit
Bit 3-15:(Reserved)
Link specific parameters.

QOS_PARAM_VAL

LINK_PARAM_GEN,
LINK_PARAM_QOS,
LINK_PARAM_GG,
LINK_ PARAM_ EDGE,
LINK_PARAM_ETH,
LINK_PARAM_802_11,
LINK_PARAM_C2K,
LINK_PARAM_FDD,
LINK_ PARAM_ HRPD,
LINK_PARAM_802_16,
LINK_PARAM_802_20,
LINK_PARAM_802_22
LINK_PARAM_INMARSAT
…. (?)

)
CHOICE(

NUM_coS_TYPES,

LIST(MIN_PK_TX_DELAY),

LIST(AVG_PK_TX_DELAY),

LIST(MAX_PK_TX_DELAY),
LIST(PK_DELAY_JITTER),
LIST(PK_LOSS_RATE),
LINK_DATA_RATE_FL,
LINK_DATA_RATE_RL,
... (?)

)

A choice of Class of Service(CoS)
parameters.

ARINC SPECIFICATION 839-Page 102

APPENDIXA ACRONYMS

APPENDIXA
ACRONYMS

3GPP

AAA

ABNF
ACARS

AC

ADN
AES
AGIE
AIS

AM

AMQP

ANS
APN

AVP

CAA

CCF

CDR
CER

CIC

CM

CM
CMU

CoS
COTS

DAL

DBP

DFO

DHCP
DLM

DNAT

DSP
DWA

DWR

EFB

FM

FOQA
FTP

GES

GHMF

GPRS

3rd Generation Partnership Project

Authentication,Authorization,and

Accounting

Augmented Backus-Naur Form
Aircraft Communications Addressing and Reporting System

Aircraft Control

Aircraft Data Network
Aircraft Earth Station
Aircraft/Ground Information Exchange

Airline Information Services

Accounting Management

Advanced Message Queuing Protocol

Aircraft Network Services
Access-Point-Name

Attribute-Value Pair

Civil Aviation Authority

Command Code Format
CallData Record
Capability-Exchange-Request

Client

Interface Controller

Central Management

Configuration Management

Communications Management Unit
Class of Service
Commercial-Off-The-Shelf

Design Assurance Level
Diameter Based Protocol

Data Flow Operations

Dynamic Host Control Protocol
Data Link Module
Dynamic Network Address Translation

Data Link Service Provider
Device-Watchdog-Answer

Device-Watchdog-Request

Electronic Flight Bag

Fault Management

Flight Operations Quality Assurance
File Transfer Protocol

Ground Earth Station
Ground-Hosted Management Function

General Packet Radio Service

ARINC SPECIFICATION 839-Page 103

APPENDIXA
ACRONYMS

Global System for Mobile Communications

Internet Assigned Numbers Authority

Internet Corporation for Assigned Names of Numbers

Interface Control Document
Internet Engineering Task Force

In-Flight Entertainment
Internet Protocol
Integrated Services Digital Network

Internet Service Provider

Information Technology

Local Area Network

Long-Term Evolution

MAGIC-Accounting-Control-Answer
MAGIC-Accounting-Control-Request

MAGIC-Accounting-Data-Answer

MAGIC-Accounting-Data-Request
Manager of Air/Ground Interface Communications

MAGIC-Client-Authentication-Answer
MAGIC-Client-Authentication-Request
Media-Dependent

Interface

MAGIC-Communication-Change-Answer

MAGIC-Communication-Change-Request

Management Flow Operations
Media Independent Handover

Media-Independent Handover Function

Media-Independent

Interface

MAGIC-Notification-Answer

MAGIC-Notification-Report

MAGIC-Status-Change-Answer

MAGIC-Status-Change-Report
MAGIC-Status-Answer

MAGIC-Status-Request
Network and Port Address Translation

Network Address Translation

Network Server System
Network Time Protocol
Policy-based Routing

Packet Data Protocol
Protocol
Passenger Information and Entertainment Services

Implementation Conformance Statement

Public Key Infrastructure

GSM

IANA

ICANN

ICD
IETF

IFE
IP
ISDN

ISP

IT

LAN

LTE

MACA
MACR

MADA

MADR
MAGIC

MCAA
MCAR
MDI

MCCA

MCCR
MFO
MIH

MIHF

MII

MNTA

MNTR

MSCA

MSCR
MSXA

MSXR
NAPT

NAT

NSS
NTP
PBR

PDP
PICS
PIES

PKI

ARINC SPECIFICATION 839-Page 104

APPENDIXA
ACRONYMS

PM

PoA

POD
QoS

REQ
RFC

SAP

SBB
SM

Performance Management

Point of Access
Passenger Owned Devices

Quality of Service

Request
Request for Comments

Service Access Point

SwiftBroadband

Security Management

SNAT

Static Network Address Transfer

STR
TFT

TLS
ToS

UDP
UTC

UTP
VPN

Session-Termination-Request
Traffic Flow Template

Transport Layer Security
Type of Service

User Datagram Protocol
Coordinated Universal Time

User Datagram Protocol
Virtual Private Network

WoW

Weight on Wheels

APPENDIX B
EXTENDED DIAMETER COMMAND STRUCTURE

ARINC SPECIFICATION 839-Page 105

APPENDIX B EXTENDED DIAMETER COMMAND STRUCTURE

The following sections show the nested AVP structure of each MAGIC defined
command pair.

B- 1 MAGIC Client Authentication Command- Pair

B-1.1

MAGIC-Client-Authentication-Request

MAGIC-Client-Authentication-Request <MCAR>:=<Diameter Header:100000,REQ>

<Session-ld>
{Origin-Host}
{Origin-Realm}
{Destination-Realm}
{Auth-Application-d}

optional [Session-Timeout ]
optional

[Client-Credentials]
{User-Name}
{Client-Password}

[Auth-Session-State]

optional
optional [Authorization-Lifetime]
[Auth-Grace-Period]
optional
optional[Destination-Host ]
optional
optional [Communication-Request-Parameters]

[REQ-Status-Info]

[Required-Bandwidth]

{Profile-Name}
optional[Requested-Bandwidth]
optional [Requested-Return-Bandwidth]
optional
optional [Required-Return-Bandwidth]
optional
optional
optional
optional[DLM-Name]
optional
optional[Flight-Phase]
optional [Altitude]
optional
optional

[Priority-Type]
[Accounting-Enabled]
[Priority-Class]

[Airport]
[TFTtoGround-List]

[QoS-Level]

1*255{TFTtoGround-Rule}

optional

[TFTtoAircraft-List]

1*255{TFTtoAircrat-Rule}

optional

[NAPT-List]

1*255{NAPT-Rule}

optional
optional
optional

[Keep-Request]
[Auto-Detect]
[Timeout]

*[AVP]

*[AVP】

ARINC SPECIFICATION 839-Page 106

APPENDIX B
EXTENDED DIAMETER COMMAND STRUCTURE

B-1.2 MAGIC-Client-Authentication-Answer

MAGIC-Client-Authentication-Answer <MCAA>::=<Diameter Header:100000>

<Session-ld>
{Result-Code}
{Origin-Host}
{Origin-Realm}
{Auth-Application-ld}
{Server-Password}
{Auth-Session-State
{Authorization-Lifetime}
{Session-Timeout
}
{Auth-Grace-Period}
{Destination-Host}
[Failed-AVP]

}

optional

1*{AVP}

optional
optional
optional
optional

[MAGIC-Status-Code]
[Error-Message]
[REQ-Status-Info]
[Communication-Answer-Parameters]

{Profile-Name}
{Granted-Bandwidth}
{Granted-Return-Bandwidth}
{Priority-Type}
{Priority-Class}
{TFTtoGround-List}

1*255{TFTtoGround-Rule}

(TFTtoAircraft-List}

1*255{TFTtoAircraft-Rule}

{QoS-Level}
{Accounting-Enabled}
{DLMAvailability-List}
{Keep-Request}
{Auto-Detect}
{Timeout}
optional[Flight-Phase]
optional
optional
optional

[Altitude]
[Airport]
[NAPT-List]

optional

1*255{NAPT-Rule}
[Gateway-IPAddress]

[AVP]

[AVP1

APPENDIX B
EXTENDED DIAMETER COMMAND STRUCTURE

ARINC SPECIFICATION 839-Page 107

B-2MAGIC Communication Change Command-Pair

B-2.1 MAGIC-Communication-Change-Request

MAGIC-Communication-Change-Request <MCCR>::=<Diameter Header:100001,REQ>

<Session-ld>
{Origin-Host}
{Origin-Realm}
{Destination-Realm}
{Communication-Request-Parameters}
{Profile-Name}

optional
optional
optional
optional
optional
optional
optional
optional
optional
optional
optional
optional
optional

[ Requested- Bandwidth]
[Requested-Return-Bandwidth]
[ Required- Bandwidth]
[ Required- Return- Bandwidth]
[Priority-Type]
[ Accounting-Enabled]
[Priority-Class]
[DLM-Name]
[QoS-Level]
[Flight-Phase]
[Altitude]
[Airport]
[TFTtoGround-List]

1*255(TFTtoGround-Rule}

optional

[ TFTtoAircraft-List]

1*255{TFTtoAircraft-Rule}

optional

[NAPT-List]

1*255{NAPT-Rule}

optional[Keep-Request]
[Auto-Detect]
optional
[Timeout]
optional

[AVP]

*[AVP]

ARINC SPECIFICATION 839-Page 108

APPENDIX B
EXTENDED DIAMETER COMMAND STRUCTURE

B-2.2 MAGIC-Communication-Change-Answer

MAGIC-Communication-Change-Answer <MCCA>::=<Diameter Header:100001>

<Session-ld>
{Result-Code}
{Origin-Host}
{Origin-Realm}
opt

[Failed-AVP]

1*

{AVP}

[MAGIC-Status-Code]
[Error-Message]

opt
opt
{Communication-Answer-Parameters}

{Profile-Name}
{Granted-Bandwidth}
{Granted-Return-Bandwidth}
{Priority-Type}
{Priority-Class}
{TFTtoGround-List}

1*255{TFTtoGround-Rule}

{TFTtoAircraft-List}

1*255{TFTtoAircraft-Rule}

{QoS-Level}
{Accounting-Enabled}
{DLM-Availability-List}
{Keep-Request}
{Auto-Detect}
{Timeout}

[Flight-Phase]
optional
[Altitude]
optional
optional
[Airport]
optional[ NAPT- List]

1*255{NAPT-Rule}

optional

[Gateway-IPAddress]

*[AVP]

*[AVP]

ARINC SPECIFICATION 839- Page 109

APPENDIX B
EXTENDED DIAMETER COMMAND STRUCTURE

B-3MAGIC Notification Command-Pair

B-3.1 MAGIC-Notification-Report

MAGIC-Notification-Report<MNTR>::=<Diameter Header:100002,REQ>

<Session-ld>
{Origin-Host}
{Origin-Realm}
{Destination-Realm}
{Communication-Report-Parameters}

{Profile-Name}

optional
optional
optional
optional
optional

[Granted-Bandwidth]
[ Granted- Return- Bandwidth]
[Priority-Type]
[Priority-Class]
[TFTtoGround-List]

1*255{TFTtoGround-Rule}

optional[TFTtoAircraft-List]

1*255{TFTtoAircraft-Rule}

optional
optional
optional

[QoS-Level]
[ DLMAvailability- List]
[NAPT-List]

optional

1*255{NAPT-Rule}
[Gateway-IPAddress]

optional
optional

[AVP]
[MAGIC-Status-Code]
[Error-Message]

*[AVP]

B-3.2 MAGIC-Notification-Answer

MAGIC-Notofication-Answer<MNTA>::=<Diameter Header:100002>

<Session-Id>
{Result-Code}
{Origin-Host}
{Origin-Realm}

optional

[Failed-AVP]
1*{AVP}

[AVP]

ARINC SPECIFICATION 839-Page 110

APPENDIX B
EXTENDED DIAMETER COMMAND STRUCTURE

B-4MAGIC Status Change Command-Pair

B-4.1 MAGIC-Status-Change-Report

MAGIC-Status-Change-Report<MSCR>::=<Diameter Header:100003,REQ>

<Session-ld>
{Origin-Host}
{Origin-Realm}
{Destination-Realm}

optional
optional
optional
optional
optional

[MAGIC-Status-Code]
[Eror-Message]
[Status-Type]
[Registered-Clients]
[DLM-List]

1*{DLM-Info}

{DLM-Name}
{DLM-Available}
{DLM-Max-Links}
{DLM-Max-Bandwith}

optional

[DLM-Max-Return-Bandwidth]

{DLM-Alocated-Links}
{DLM-Alocated-Bandwith}

optional

[ DLM- Allocated- Retum- Bandwidth]

{DLM-QoS-Level-List}
1*3{QoS-Level}
[DLM-Link-Status-List]

optional

1*{L

ink-Status-Group}
{Link-Name}
{Link-Number}
{Link-Available}
{QoS-Level}
{Link-Connection-Status}
{Link-Login-Status}
{Link-Max-Bandwidth}

optional

[ Link- Max- Returm- Bandwidth]

{Link-Aloc-Bandwidth}

optional
optional

[ Link- Alloc- Return- Bandwith]
[Link-Eror-String]

[AVP]

[AVP1

[AVP]

B-4.2 MAGIC-Status-Change-Answer

MAGIC-Status-Change-Answer<MSCA>::=<Diameter Header:100003>

<Session-ld>
(Resul-Code}
{Origin-Host}
{Origin-Realm}

optional

[Failed-AVP]
1*{AVP}

*[AVP]

ARINC SPECIFICATION 839-Page 111

APPENDIX B
EXTENDED DIAMETER COMMAND STRUCTURE

B-5MAGIC Status Command-Pair

B-5.1 MAGIC-Status-Request

MAGIC-Status-Request<MSXR>::=<Diameter Header:100004,REQ>

<Session-ld>
{Origin-Host}
{Origin-Realm}
{Destination-Realm}
{Status-Type}
[AVP]

B-5.2 MAGIC-Status-Answer

MAGIC-Status-Answer<MSXA>::=<Diameter Header:100004>

<Session-Id>
{Result-Code}
{Origin-Host}
{Origin-Realm}
{Status-Type}

optional
optional
optional

[MAGIC-Status-Code]
[Error-Message]
[Failed-AVP]
1*{AVP}

optional
optional[DLMList]

[Registered-Clients]

1*{DLM-Info}

{DLMName}
{DLM-Available}
{DLM-Max-Links}
{DLM-Max-Bandwith}

optional

[DLM-Max-Return-Bandwidth]

{DLM-Alocated-Links}
{DLM-Alocated-Bandwith}

optional

[ DLM- Allocated- Return- Bandwidth]

{DLM-QoS-Level-List}
1*3{QoS-Level}
[DLM-Link-Status-List]

optional

1*{L ink-Status-Group}

{Link-Name}
{Link-Number}
{Link-Available}
{QoS-Level}
{Link-Connection-Status}
{Link-Login-Status}
{Link-Max-Bandwidth}

optional

[ Link- Max- Return- Bandwidth]

{Link-Alloc-Bandwidth}

optional
optional

[ Link-Alloc- Return- Bandwith]
[Link-Error-String]

*[AVP]

[AVPJ

*[AVP]

ARINC SPECIFICATION 839-Page 112

APPENDIX B
EXTENDED DIAMETER COMMAND STRUCTURE

B-6MAGIC Accounting-Data Command-Pair

B-6.1 MAGIC-Accounting-Data-Request

MAGIC-Accounting-Data-Request<MADR>::=<Diameter Header:100005,REQ>

<Session-ld>
{Origin-Host}
{Origin-Realm}
{Destination-Realm}
{CDR-Type}
{CDR-Level}

optional

[CDR-Request-dentifier]

*[AVP]

B-6.2 MAGIC-Accounting-Data-Answer

MAGIC-Accounting-Data-Answer<MADA>::=<Diameter Header:100005>

<Session-Id>
{Result-Code}
{Origin-Host}
{Origin-Realm}
{CDR-Type}
{CDR-Level}

optional
optional

[ CDR- Request- dentifier]
[CDRs-Active]

1*{CDR-Info}

{CDR-Id}

optional

[CDR-Content]

optional

[CDRs-Finished]

1*{CDR-Info}

{CDR-Id}

optional

[CDR-Content]

optional

[CDRs-Forwarded]

1*{CDR-Info}

{CDR-Id}

optional

[CDR-Content]

optional

optional
optional
optional

[CDRs-Unknown]
1*{CDR-d}
[MAGIC-Status-Code]
[Error-Message]
[Failed-AVP]
1*{AVP}

[AVP]

B-7 MAGIC Accounting Control Command-Pair
B-7.1 MAGIC-Accounting-Control-Request

MAGIC-Accounting-Control-Request<MACR>::=<Diameter Header:100006,REQ>

<Session-ld>
{Origin-Host}
{Origin-Realm}
{CDR-Restart-Session-d}
[AVP]

APPENDIX B
EXTENDED DIAMETER COMMAND STRUCTURE

B-7.2 MAGIC-Accounting-Control-Answer

MAGIC-Accounting-Control-Answer<MACA>::=<Diameter Header:100006>

ARINC SPECIFICATION 839-Page 113

<Session-Id>
{Result-Code}
{Origin-Host}
{Origin-Realm}
{CDR-Restart-Session-ld}
[MAGIC-Status-Code]
[Error-Message]
[Failed-AVPJ
1*{AVP}

optional
optional
optional

optional

[CDRs-Updated]

1*{CDR-Start-Stop-Pair}

{CDR-Stoped}
{CDR-Started}

*[AVP]

ARINC SPECIFICATION 839-Page 114

APPENDIX C
GUIDANCE WITH RESPECT TO AIRCRAFT DOMAIN REFERENCE MODEL

APPENDIX C GUIDANCE WITH RESPECT TO AIRCRAFT DOMAIN REFERENCE MODEL

The aircraft domain reference model described in ARINC Specification 664:
Aircraft Data Network provides a notional model of the aircraft and divisions among

four domains:

1.Aircraft Control
2.Airline Information Services (AIS)domain

(AC)domain

3.Passenger

Information and Entertainment Services(PIES)domain

4.Passenger Owned

Devices(PODs)domain

Segregation principles applicable to aircraft network domains as described in the
following documents:

ARINC Report 82 1: Aircraft Network Server System( NSS) Functional Definition

ARINC Specification 664: Aircraft Data Network,Part 5,Network Domain
Characteristics and Interconnection.

Aircraft network domain concepts represent a broad description of functions that are
present aboard the aircraft.Though many lower level functions may be found in any
one domain,it
generally describe where the function should reside and at what level of certification
may be required.

is useful to consider the broader description of categories as they

ARINC SPECIFICATION 839-Page 115

APPENDIX D
USE CASES

APPENDIX D

USE CASES

This document was prepared from information contained in a variety of example use
cases discussed by industry.By their very nature,use cases may be
implementation specific and they are subject to change.Uses cases are not
intended to be part of this standard.However,the following example illustrates why
this appendix is included in this document.

A use case is provided from an operational point of view.It provides an example of
how data logging for the airborne side in a common format would facilitate a security
review.This is becoming more necessary as the IT airline function needs to provide
the requirements to the maintenance and engineering function that has access and
responsibility for the aircraft.

ARINC Standard-Errata Report

1.Document

Title

(Insert the number,supplement level,date ofpublication,and title ofthe document with the error)

2. Reference

Page Number:

Section Number:

Date of Submission:

3.Error

(Reproduce the material in error,as it appears in the standard.)

4.Recommended

Correction

(Reproduce the correction as it would appear in the corrected version ofthe material.)

5.Reason for Correction(Optional)

(State why the correction is necessary.)

6.Submitter

(Optional)

(Name,organization,contact information,e.g.,phone,email address.)

Please

return

comments

to

standards@sae-itc.com

Note:Items 2-5may be repeated for additional errata.All recommendations will be evaluated by the staff.Any
substantive changes will require submission to the relevant subcommittee for incorporation into a subsequent
Supplement.

Errata Report Identifier:

Engineer Assigned:

[To be completed by IA Staff]

Review Status:

ARINC Standard Errata Form
June 2014

Project

Initiation/Modification proposal
Date Proposed:Click

the AEEC
here to enter a date.

for

1.0

1.1

2.0
2.1

2.2

ARINC Project Initiation/Modification(APIM)

Name of Proposed Project
(Insert name of proposed project.)

APIM#:

Name of Originator and/or Organization
(Insert name of individual and/or the organization that initiated the APIM)

Subcommittee Assignment and Project Support

Suggested AEEC Group and Chairman
(Identify an existing or new AEEC group.)

Support for the activity(as verified)
Airlines:(Identify each company by name.)

Airframe Manufacturers:
Suppliers:
Others:

2.3

Commitment for Drafting and Meeting Participation (as verified)

2.4

3.0

3.1

3.2

Airlines:
Airframe Manufacturers:
Suppliers:
Others:

Recommended Coordination with other groups
(List other AEEC subcommittees or other groups.)

Project Scope (why and when standard is needed)

Description

(Insert description of the scope of the project.)

Planned usage of the envisioned specification
Note:New airplane programs must be confirmed by manufacturer prior to

completing this section.

New aircraft developments planned to use this specification
(aircraft &date)
(aircraft &date)
(manufacturer,aircraft &date)

Airbus:
Boeing:
Other:

Modification/retrofit requirement

Specify:

(aircraft &date)

yes □ no □

yes □ no □

Needed for airframe manufacturer or airline project

yes □ no □

Specify:

(aircraft &date)

Updated:June

Page 1 of 3
2014

Mandate/regulatory requirement

yes □ no □

Program and date:(program &date)

Is the activity defining/changing an infrastructure standard?

yes □no □

Specify

(e.g.,ARINC 429)

When is the ARINC standard required?

(month/year)

What is driving this date?

(state reason)

Are 18 months(min)available for standardization work?

yes □ no □

If NO please specify solution:

Are Patent(s)involved?

If YES please describe,identify patent holder:

Issues to be worked
(Describe the major issues to be addressed.)

Benefits

Basic benefits
Operational enhancements
For equipment standards:
(a)Is this a hardware characteristic?

(b)Is this a software characteristic?

(c)Interchangeable interface definition?
(d)Interchangeable function definition?

yes □ no □

yes □ no □

yes □ no □

yes □ no □

yes □ no □
yes □ no □

If not fully interchangeable,please explain:

Is this a software interface and protocol standard?

yes □ no □

Specify:

Product offered by more than one supplier

yes □ no □

Identify:

(company name)

Specific project benefits(Describe overall project benefits.)

Benefits for Airlines
(Describe any benefits unique to the airline point of view.)

Benefits for Airframe Manufacturers
(Describe any benefits unique to the airframe manufacturer's point of view.)

Benefits for Avionics Equipment Suppliers
(Describe any benefits unique to the equipment supplier's point of view.)

Documents to be Produced and Date of Expected Result
Identify Project Papers expected to be completed per the table in the following
section.

3.3

4.0

4.1

4.2
4.2.1

4.2.2

4.2.3

5.0

Page 2 of 3
Updated:June 2014

5.1

Meetings and Expected Document Completion
The following table identifies the number of meetings and proposed meeting days
needed to produce the documents described above.

Activity

Mtgs

Document a

#of mtgs

Mtg-Days
(Total)
#of mtg days

Expected Start
Date
mm/yyyy

Expected
Completion Date
mm/yyyy

Document b

#of mtgs

#of mtg days

mm/yyyy

mm/yyy

Please note the number of meetings,the number of meeting days,and the
frequency of web conferences to be supported by the IA Staff.

Comments
(Insert any other information deemed useful to the committee for managing this
work.)

Expiration Date for the APIM
April/October 20XX

6.0

6.1

Completed forms should be submitted to theAEEC Executive Secretary.

Page 3 of 3
Updated:June 2014

