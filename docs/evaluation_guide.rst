Evaluation Guide
================

This guide provides the competition judges with a direct mapping of project 
deliverables to the **ESA-CCSDS Delivery Outline** requirements.

🛰️ Standards Compliance (Milestone 1)
--------------------------------------

*   **Compliance Matrix**: See :doc:`signal/compliance_matrix` for a line-by-line 
    mapping of implemented modules to the LSIS standard.
*   **Proposed Resolutions**: See :doc:`signal/spec_findings_report` for our 
    identification of spec ambiguities and proposed formal resolutions.
*   **Architecture**: See :doc:`signal/architecture` for the high-level 
    design of the C++ and Python layers.

🛠️ Protocol Implementation (Milestone 2)
----------------------------------------

*   **Mission Assurance**: See :doc:`assurance/mission_assurance` for our 
    implementation of NASA, ESA, and JAXA high-reliability patterns 
    (Soft-TMR, CFI, etc.).
*   **BCH Decoding**: See :doc:`signal/bch_decoder_design` for the rationale 
    behind our radiation-hardened Maximum Likelihood decoder.
*   **Implementation Status**: See :doc:`roadmap` for the
    component-level delivery status of the full transmit + decode chain.
*   **Deliverable Verification**: :doc:`gateway0` through :doc:`gateway7`
    give per-gateway evidence with the exact commands a judge can run to
    verify each deliverable.

🔗 Validation & Interoperability (Milestone 3)
----------------------------------------------

*   **Interoperability Results**: See :doc:`interop_results` — bit-exact
    cross-checks and a signal cross-decode against LANS-AFS-SIM, an
    independent third-party implementation, reproducible with one command
    (``task interop``).  Shared vectors for other teams are published at
    `luar-space/lsis-afs-test-vectors
    <https://github.com/luar-space/lsis-afs-test-vectors>`__.
*   **Interoperability Standard**: See :doc:`test_vectors` for the
    LunaLink binary interchange format for cross-team verification.
*   **Performance Evidence**: :doc:`signal/ldpc_performance` and
    :doc:`signal/bch_performance` record the BER/FER waterfalls;
    ``task performance`` reproduces the WCET timing and channel
    acceptance runs.
*   **Type Safety**: All interfaces utilize semantic wrappers (``Fid``,
    ``Toi``, ``PrnId``) to prevent human-in-the-loop errors.
*   **Verification Evidence**: 100% Python line coverage is CI-gated;
    C++ line/branch coverage and MC/DC evidence are tracked per
    :doc:`assurance/mission_assurance`.
