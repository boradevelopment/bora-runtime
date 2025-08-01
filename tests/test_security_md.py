"""
Unit tests for validating the SECURITY.md file structure and content.
Using pytest framework for comprehensive testing of security policy documentation.

This test suite validates the SECURITY.md file that contains:
- Security Policy header
- Basic content validation
- Markdown format compliance
- File structure and accessibility
"""

import re
import time
from pathlib import Path

# Using pytest as the testing framework based on standard Python testing practices
try:
    import pytest
except ImportError:
    # Fallback to unittest if pytest is not available
    import unittest
    pytest = None


class TestSecurityMd:
    """Test class for SECURITY.md file validation."""
    
    def setup_method(self):
        """Setup method to initialize test data."""
        self.security_file_path = Path("SECURITY.md")
        self.security_content = None
        if self.security_file_path.exists():
            try:
                self.security_content = self.security_file_path.read_text(encoding='utf-8')
            except (UnicodeDecodeError, IOError):
                self.security_content = None
    
    def test_security_file_exists(self):
        """Test that SECURITY.md file exists in the repository root."""
        assert self.security_file_path.exists(), "SECURITY.md file should exist in repository root"
    
    def test_security_file_not_empty(self):
        """Test that SECURITY.md file is not empty."""
        assert self.security_content is not None, "SECURITY.md file should exist and be readable"
        assert self.security_content.strip(), "SECURITY.md file should not be empty"
    
    def test_security_file_has_header(self):
        """Test that SECURITY.md file has a proper security policy header."""
        if self.security_content:
            assert "# Security Policy" in self.security_content, \
                "SECURITY.md should have '# Security Policy' header"
    
    def test_security_file_encoding(self):
        """Test that SECURITY.md file uses UTF-8 encoding."""
        if self.security_file_path.exists():
            try:
                self.security_file_path.read_text(encoding='utf-8')
            except UnicodeDecodeError:
                raise AssertionError("SECURITY.md should use UTF-8 encoding") from None
    
    def test_security_file_line_endings(self):
        """Test that SECURITY.md file uses consistent line endings."""
        if self.security_content:
            # Check for mixed line endings
            has_crlf = '\r\n' in self.security_content
            has_lf = '\n' in self.security_content.replace('\r\n', '')
            
            assert not (has_crlf and has_lf), "SECURITY.md should use consistent line endings"
    
    def test_security_file_markdown_syntax(self):
        """Test basic markdown syntax validation."""
        if self.security_content:
            lines = self.security_content.split('\n')
            
            for line_num, line in enumerate(lines, 1):
                # Check for balanced brackets in markdown links
                if '[' in line and ']' in line:
                    bracket_count = line.count('[') - line.count(']')
                    assert bracket_count == 0, f"Line {line_num}: Unbalanced brackets in markdown"
                
                # Check markdown link syntax
                if '](' in line:
                    # Find all markdown links
                    link_pattern = r'\[([^\]]*)\]\(([^)]*)\)'
                    matches = re.findall(link_pattern, line)
                    assert len(matches) > 0, f"Line {line_num}: Invalid markdown link syntax"
    
    def test_security_file_contains_security_keywords(self):
        """Test that SECURITY.md contains security-related keywords."""
        if self.security_content:
            content_lower = self.security_content.lower()
            assert 'security' in content_lower, "SECURITY.md should contain 'security' keyword"
            assert 'policy' in content_lower, "SECURITY.md should contain 'policy' keyword"
    
    def test_security_file_no_sensitive_info(self):
        """Test that SECURITY.md doesn't contain sensitive information patterns."""
        if self.security_content:
            # Patterns that might indicate accidentally exposed sensitive information
            sensitive_patterns = [
                r'password\s*[:=]\s*\S+',  # Password assignments
                r'api[_-]?key\s*[:=]\s*\S+',  # API key assignments
                r'secret\s*[:=]\s*\S+',  # Secret assignments
                r'token\s*[:=]\s*[a-zA-Z0-9]{20,}',  # Long tokens
                r'-----BEGIN\s+PRIVATE\s+KEY-----',  # Private keys
            ]
            
            for pattern in sensitive_patterns:
                matches = re.findall(pattern, self.security_content, re.IGNORECASE)
                assert not matches, f"SECURITY.md should not contain sensitive information: {matches}"
    
    def test_security_file_reasonable_length(self):
        """Test that SECURITY.md has reasonable content length."""
        if self.security_content:
            word_count = len(self.security_content.split())
            assert word_count >= 3, "SECURITY.md should have at least a few words of content"
            assert word_count <= 10000, "SECURITY.md should not be excessively long (>10000 words)"
    
    def test_security_file_permissions_readable(self):
        """Test that SECURITY.md file has appropriate read permissions."""
        if self.security_file_path.exists():
            file_stat = self.security_file_path.stat()
            # Check if file is readable by owner
            assert file_stat.st_mode & 0o400, "SECURITY.md should be readable by owner"
    
    def test_security_content_specific_validation(self):
        """Test the specific content provided in the diff."""
        # Validate the actual content from the file matches expected structure
        expected_header = "# Security Policy"
        expected_content_line = "Sooner, testing pull request"
        
        if self.security_content:
            assert expected_header in self.security_content, "Should contain the Security Policy header"
            assert expected_content_line in self.security_content, "Should contain the expected content line"
            
            # Validate structure
            lines = [line.strip() for line in self.security_content.strip().split('\n') if line.strip()]
            assert len(lines) >= 2, "Security policy should have header and content"
            
            # First line should be the header
            assert lines[0] == expected_header, "First line should be the security policy header"
            
            # Should have content beyond just the header
            content_lines = [line for line in lines if not line.startswith('#')]
            assert len(content_lines) > 0, "Security policy should have content beyond headers"
    
    def test_security_markdown_headers_format(self):
        """Test that markdown headers follow proper formatting."""
        if self.security_content:
            lines = self.security_content.split('\n')
            
            for line_num, line in enumerate(lines, 1):
                if line.startswith('#'):
                    # Check for space after hash symbols (except for single #)
                    hash_count = 0
                    for char in line:
                        if char == '#':
                            hash_count += 1
                        else:
                            break
                    
                    # If there's content after the hashes, should have a space
                    if len(line) > hash_count and line[hash_count] != ' ':
                        raise AssertionError(f"Line {line_num}: Header should have space after # symbols")
    
    def test_security_file_basic_structure(self):
        """Test that the security file has a basic expected structure."""
        if self.security_content:
            # Should start with a header
            lines = self.security_content.strip().split('\n')
            assert lines[0].startswith('#'), "Security file should start with a markdown header"
            
            # Should contain the word "Security" in the first line
            assert 'Security' in lines[0], "First line should mention Security"
    
    def test_security_file_is_markdown(self):
        """Test that the file appears to be valid markdown format."""
        assert self.security_file_path.suffix.lower() == '.md', "Security policy should be a markdown file"
    
    def test_security_content_not_placeholder(self):
        """Test that the content is not just a placeholder or template."""
        if self.security_content:
            placeholder_indicators = [
                'todo',
                'tbd',
                'placeholder',
                'fill in',
                'replace this',
                '[your',
                '{{',
            ]
            
            content_lower = self.security_content.lower()
            for indicator in placeholder_indicators:
                assert indicator not in content_lower, \
                    f"SECURITY.md appears to contain placeholder text: '{indicator}'"
    
    def test_security_file_modification_reasonable(self):
        """Test that SECURITY.md file has reasonable modification time."""
        if self.security_file_path.exists():
            file_mtime = self.security_file_path.stat().st_mtime
            current_time = time.time()
            
            # File should not be from the future
            assert file_mtime <= current_time + 86400, \
                "SECURITY.md modification time should not be in the future"
            
            # File should not be older than 10 years (reasonable for any security policy)
            ten_years_ago = current_time - (10 * 365 * 24 * 60 * 60)
            assert file_mtime > ten_years_ago, \
                "SECURITY.md seems very old, consider updating security policy"
    
    def test_security_content_diff_validation(self):
        """Test the specific content changes from the pull request diff."""
        # This test validates the exact content that was added in the diff
        expected_full_content = "# Security Policy\nSooner, testing pull request\n"
        
        if self.security_content:
            # Remove any trailing whitespace for comparison
            normalized_content = self.security_content.rstrip() + '\n'
            assert normalized_content == expected_full_content, \
                "SECURITY.md content should match the expected diff content exactly"
    
    def test_security_file_has_minimal_required_sections(self):
        """Test that security file has at minimum the required sections."""
        if self.security_content:
            # At minimum should have a policy section
            lines = [line.strip() for line in self.security_content.split('\n') if line.strip()]
            
            # Should have at least header and one content line
            assert len(lines) >= 2, "Security policy should have header and at least one content line"
            
            # Check that it's not just empty headers
            non_header_content = [line for line in lines if not line.startswith('#')]
            assert len(non_header_content) >= 1, "Security policy should have substantive content"
    
    def test_security_content_format_consistency(self):
        """Test that the security file follows consistent formatting."""
        if self.security_content:
            lines = self.security_content.split('\n')
            
            # Check for consistent use of markdown headers
            header_lines = [line for line in lines if line.startswith('#')]
            for header in header_lines:
                # Headers should follow "# Title" format
                if len(header) > 1:
                    assert header[1] == ' ', f"Header '{header}' should have space after #"
    
    def test_security_file_character_encoding_safety(self):
        """Test that the file doesn't contain problematic characters."""
        if self.security_content:
            # Check for potentially problematic characters
            problematic_chars = ['\x00', '\x01', '\x02', '\x03', '\x04', '\x05']
            
            for char in problematic_chars:
                assert char not in self.security_content, \
                    f"SECURITY.md should not contain null or control characters: {repr(char)}"
    
    def test_security_policy_completeness_indicators(self):
        """Test for indicators that the security policy is reasonably complete."""
        if self.security_content:
            content_lower = self.security_content.lower()
            
            # Should contain some indication of how to handle security issues
            completeness_indicators = [
                'policy',
                'security',
                'report',
                'contact',
                'issue',
                'vulnerability',
                'bug',
                'disclosure',
            ]
            
            # Should contain at least 2 of these indicators for a meaningful policy
            found_indicators = sum(1 for indicator in completeness_indicators 
                                 if indicator in content_lower)
            
            assert found_indicators >= 2, \
                "Security policy should contain multiple relevant security terms"


# Fallback for unittest if pytest is not available
if pytest is None:
    import unittest
    
    class TestSecurityMdUnittest(unittest.TestCase):
        """Unittest fallback for SECURITY.md validation."""
        
        def setUp(self):
            """Set up test fixtures."""
            self.security_file_path = Path("SECURITY.md")
            self.security_content = None
            if self.security_file_path.exists():
                try:
                    self.security_content = self.security_file_path.read_text(encoding='utf-8')
                except (UnicodeDecodeError, IOError):
                    self.security_content = None
        
        def test_security_file_exists(self):
            """Test that SECURITY.md file exists."""
            self.assertTrue(self.security_file_path.exists(), 
                            "SECURITY.md file should exist in repository root")
        
        def test_security_file_has_content(self):
            """Test that SECURITY.md has expected content."""
            self.assertIsNotNone(self.security_content, "SECURITY.md should be readable")
            self.assertIn("# Security Policy", self.security_content, 
                          "Should contain Security Policy header")
            self.assertIn("Sooner, testing pull request", self.security_content,
                          "Should contain expected content")
        
        def test_security_basic_structure(self):
            """Test basic structure requirements."""
            if self.security_content:
                lines = self.security_content.strip().split('\n')
                self.assertGreaterEqual(len(lines), 2, 
                                      "Should have header and content")
                self.assertTrue(lines[0].startswith('#'), 
                              "Should start with markdown header")


if __name__ == '__main__':
    if pytest is not None:
        # Run with pytest if available
        pytest.main([__file__])
    else:
        # Fallback to unittest
        unittest.main()