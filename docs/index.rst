LunaLink
========

.. raw:: html

   <p>
     <img src="https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/fiskio/139670230055e3018c1d8fbcac0d4eea/raw/lunalink-python-coverage.json" alt="Python Coverage">
     <img src="https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/fiskio/139670230055e3018c1d8fbcac0d4eea/raw/lunalink-region-coverage.json" alt="Region Coverage">
     <img src="https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/fiskio/139670230055e3018c1d8fbcac0d4eea/raw/lunalink-line-coverage.json" alt="Line Coverage">
     <img src="https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/fiskio/139670230055e3018c1d8fbcac0d4eea/raw/lunalink-branch-coverage.json" alt="Branch Coverage">
     <img src="https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/fiskio/139670230055e3018c1d8fbcac0d4eea/raw/lunalink-mcdc-coverage.json" alt="MC/DC Coverage">
     <img src="https://img.shields.io/badge/license-Apache%202.0-green.svg" alt="License: Apache 2.0">
   </p>

An open-source reference implementation of the **LunaNet Signal-In-Space
Recommended Standard for the Augmented Forward Signal** (LSIS V1.0,
29 January 2025), the navigation signal that lunar service providers will
broadcast in the 2483.5-2500 MHz band.

LunaLink implements the standard in both directions. The transmit chain takes
navigation data through framing, forward error correction, spreading and
modulation to a 12-second signal. The receive chain takes that signal back to
typed navigation values, through synchronisation, soft-decision decoding and
message parsing. A C++20 core does the signal processing; Python bindings make
it usable from a notebook.

Two things make it more than a demonstration. It is written as flight software,
in a restricted C++ dialect with an assurance case behind it. And it
interoperates with independent implementations in both directions, which is
checked on every run rather than asserted.

Where to start
--------------

To use LunaLink, build it and run the tests:

.. code-block:: bash

   task install     # build the core and the Python package
   task test        # C++ and Python test suites
   task interop     # cross-check against an independent implementation

:doc:`quickstart` then walks through generating and decoding a frame, and
:doc:`signal/architecture` explains how the chain fits together.

To check what LunaLink claims, start at :doc:`verifying`.  It gives, for each
claim, the page that documents it and the command that reproduces the
evidence.

To check your own LSIS-AFS implementation against LunaLink, start at
:doc:`interoperate`.  The shared test vectors and their validator are
published at `luar-space/lsis-afs-test-vectors
<https://github.com/luar-space/lsis-afs-test-vectors>`__, and comparing
against them does not require installing LunaLink.

:doc:`roadmap` lists what is not done.

.. toctree::
   :maxdepth: 2
   :caption: Getting Started:

   quickstart
   verifying
   interoperate
   developing
   tech_stack
   roadmap

.. toctree::
   :maxdepth: 2
   :caption: Tutorials:

   notebooks/01_spreading_codes
   notebooks/02_error_correction
   notebooks/03_navigation_message

.. toctree::
   :maxdepth: 2
   :caption: The Signal:

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
   :caption: Conformance & Interoperability:

   signal/compliance_matrix
   signal/shall_inventory
   signal/pics_proforma
   signal/spec_findings_report
   signal/validation_plan
   interop_results
   test_vectors

.. toctree::
   :maxdepth: 2
   :caption: Mission Assurance:

   assurance/mission_assurance
   assurance/c_plus_plus_guidelines
   assurance/fmea
   assurance/fault_coverage
   assurance/traceability_matrix
   assurance/coverage_evidence
   assurance/coverage_exclusions
   assurance/metrics_plan
   assurance/build_policy
   assurance/configuration_baseline
   assurance/tool_confidence

.. toctree::
   :maxdepth: 2
   :caption: Verification by Block:

   verification/spreading_codes
   verification/error_correction
   verification/framing
   verification/baseband
   verification/sync_decoding
   verification/message_parsing
   verification/integration
   verification/performance

.. toctree::
   :maxdepth: 2
   :caption: Process & Standards:

   governance/sdp
   governance/scmp
   governance/baseline_register
   standards/esa/ecss_standards
   standards/nasa/npr7150_standards
   standards/nasa_compliance_addendum
   versioning
   references

.. toctree::
   :maxdepth: 2
   :caption: API Reference:

   api
   api_cpp

.. toctree::
   :maxdepth: 2
   :caption: Project History:

   history/index

Licence and status
------------------

LunaLink is licensed under Apache-2.0. It implements a standard it does not
define, and it is not endorsed by NASA, ESA, CCSDS or any LunaNet service
provider. Nothing here has flown. See ``GOVERNANCE.md`` in the repository for
what that means in practice.
