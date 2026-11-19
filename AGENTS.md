Code formatting rules:
* In if statements and similar constructs, always use braces `{}` even for single-line blocks to improve readability and prevent errors.
* TODO: look up wpiformat style guides and incorporate them into the project's code formatting rules.
* When writing tests, split up test cases into small, focused units that each test a single aspect of the functionality. This improves test readability, makes it easier to identify the source of failures, and ensures that tests remain maintainable as the codebase evolves.
* Always aim for clear and descriptive test names that convey the purpose and expected outcome of the test. This helps other developers quickly understand the intent of the test and facilitates easier debugging when tests fail.
* When writing tests, prefer using setup and teardown methods to initialize and clean up test environments. This ensures that each test runs in a consistent state and reduces the risk of test interference or side effects.
* Avoid hardcoding values in tests whenever possible. Use constants, configuration files, or test data generators to make tests more maintainable and adaptable to changes in the codebase.
* When writing tests, you MUST write so that a segfault does not trigger a UI popup or any other unintended side effects. Ensure that tests handle errors gracefully and that any potential crashes are properly contained and reported within the test framework.
* Ensure that tests are independent and do not rely on the execution order of other tests. This prevents cascading failures and makes it easier to run tests in isolation.
* Regularly review and refactor tests to remove redundancy, improve clarity, and keep them aligned with the evolving codebase.
* Use descriptive assertions and error messages in tests to make it clear what is being verified and why a test might fail. This aids in debugging and understanding test failures quickly.
* When testing edge cases and error conditions, explicitly include tests for invalid inputs, boundary values, and exceptional scenarios. This ensures that the code handles unexpected situations gracefully and reduces the likelihood of hidden bugs.
* Continuously update and expand the test suite as new features are added or existing functionality changes. This helps maintain high test coverage and ensures that the codebase remains robust against regressions.

This app does not need to be tested for mobile platforms, as it is primarily designed for desktop environments and its functionality is not dependent on mobile-specific features or behaviors.