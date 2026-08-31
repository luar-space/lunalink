Evaluation Guide
================

This guide provides the competition judges with a direct mapping of project 
deliverables to the **ESA-CCSDS Delivery Outline** requirements.

🛰️ Standards Compliance (Milestone 1)
--------------------------------------

*   **Compliance Matrix**: See :doc:`signal/compliance_matrix` for a line-by-line 
    mapping of implemented modules to the LSIS standard.
*   **Proposed Resolutions**: See :doc:`signal/spec_findings_report` for our 
    identificiation of spec ambiguities and proposed formal resolutions.
*   **Architecture**: See :doc:`signal/architecture` for the high-level 
    design of the C++ and Python layers.

🛠️ Protocol Implementation (Milestone 2)
----------------------------------------

*   **Mission Assurance**: See :doc:`assurance/mission_assurance` for our 
    implementation of NASA, ESA, and JAXA high-reliability patterns 
    (Soft-TMR, CFI, etc.).
*   **BCH Decoding**: See :doc:`signal/bch_decoder_design` for the rationale 
    behind our radiation-hardened Maximum Likelihood decoder.
*   **Implementation Roadmap**: See :doc:`roadmap` for the detailed 
    component-level status of the V3 Full Codec implementation.

🔗 Validation & Interoperability (Milestone 3)
----------------------------------------------

*   **Interoperability Standard**: See :doc:`test_vectors` for the 
    LunaLink binary interchange format for cross-team verification.
*   **Type Safety**: All interfaces utilize semantic wrappers (``Fid``, 
    ``Toi``, ``PrnId``) to prevent human-in-the-loop errors.
*   **Verification Evidence**: The repository maintains 100% Python 
    coverage and 98% C++ coverage across multiple hardware-aligned CI jobs.
