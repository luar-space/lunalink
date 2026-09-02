LunaLink — LSIS-AFS Reference Implementation
============================================

.. raw:: html

   <p>
     <img src="https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/fiskio/139670230055e3018c1d8fbcac0d4eea/raw/lunalink-python-coverage.json" alt="Python Coverage">
     <img src="https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/fiskio/139670230055e3018c1d8fbcac0d4eea/raw/lunalink-region-coverage.json" alt="Region Coverage">
     <img src="https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/fiskio/139670230055e3018c1d8fbcac0d4eea/raw/lunalink-line-coverage.json" alt="Line Coverage">
     <img src="https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/fiskio/139670230055e3018c1d8fbcac0d4eea/raw/lunalink-branch-coverage.json" alt="Branch Coverage">
     <img src="https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/fiskio/139670230055e3018c1d8fbcac0d4eea/raw/lunalink-mcdc-coverage.json" alt="MC/DC Coverage">
     <img src="https://img.shields.io/badge/license-Apache%202.0-green.svg" alt="License: Apache 2.0">
   </p>

Open-source reference implementation of the LunaNet Signal-In-Space
Augmented Forward Signal (LSIS V1.0, 29 January 2025): the complete
transmit chain (spreading codes, BCH + LDPC FEC, framing, BPSK/IQ
modulation), the inverse decode chain, bit-exact interoperability evidence
against an independent implementation, and flight-software-grade assurance
(SWIFI fault injection, FMEA, MC/DC coverage).  C++20 core with Python
bindings.

New here?  Start with the :doc:`quickstart`; judging the competition
submission?  Start with the :doc:`evaluation_guide`.

.. toctree::
   :maxdepth: 2
   :caption: Executive Summary:

   evaluation_guide
   quickstart
   roadmap
   references

.. toctree::
   :maxdepth: 2
   :caption: Deliverable Verification:

   gateway0
   gateway1
   gateway2
   gateway3
   gateway4
   gateway5
   gateway6
   gateway7
   gateway8
   interop_results

.. toctree::
   :maxdepth: 2
   :caption: Mission Assurance:

   assurance/mission_assurance
   assurance/c_plus_plus_guidelines
   assurance/traceability_matrix
   assurance/build_policy
   assurance/configuration_baseline
   assurance/tool_confidence
   assurance/fmea
   assurance/fault_coverage
   assurance/metrics_plan
   assurance/coverage_evidence
   assurance/coverage_exclusions
   signal/compliance_matrix
   signal/shall_inventory
   signal/pics_proforma
   signal/spec_findings_report
   signal/validation_plan

.. toctree::
   :maxdepth: 2
   :caption: Governance:

   governance/sdp
   governance/scmp
   governance/baseline_register

.. toctree::
   :maxdepth: 2
   :caption: Standards & Compliance:

   standards/esa/ecss_standards
   standards/nasa/npr7150_standards
   standards/nasa_compliance_addendum

.. toctree::
   :maxdepth: 2
   :caption: Technical Reference:

   signal/architecture
   signal/signal_chain
   signal/algorithms
   signal/bch_decoder_design
   signal/bch_performance
   signal/ldpc_performance
   signal/ldpc_algo_card
   signal/sisicd
   signal/icd

.. toctree::
   :maxdepth: 2
   :caption: Tutorials:

   notebooks/01_spreading_codes
   notebooks/02_error_correction
   notebooks/03_navigation_message

.. toctree::
   :maxdepth: 2
   :caption: Developer & Interoperability:

   developing
   tech_stack
   test_vectors
   versioning
   api
   api_cpp
