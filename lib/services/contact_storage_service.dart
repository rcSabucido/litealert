import 'dart:convert';
import 'package:shared_preferences/shared_preferences.dart';

class ContactStorageService {
  static const String _contactsKey = 'contacts';

  // Save contacts to local storage
  Future<void> saveContacts(List<Map<String, String>> contacts) async {
    final prefs = await SharedPreferences.getInstance();
    final contactsJson = contacts.map((contact) => jsonEncode(contact)).toList();
    await prefs.setStringList(_contactsKey, contactsJson);
  }

  // Load contacts from local storage
  Future<List<Map<String, String>>> loadContacts() async {
    final prefs = await SharedPreferences.getInstance();
    final contactsJson = prefs.getStringList(_contactsKey) ?? [];
    return contactsJson.map((jsonString) {
      final Map<String, dynamic> decoded = jsonDecode(jsonString);
      return decoded.map((key, value) => MapEntry(key, value.toString()));
    }).toList();
  }

  // Add a single contact
  Future<void> addContact(Map<String, String> contact) async {
    final contacts = await loadContacts();
    contacts.add(contact);
    await saveContacts(contacts);
  }

  // Delete a contact by index
  Future<void> deleteContact(int index) async {
    final contacts = await loadContacts();
    if (index >= 0 && index < contacts.length) {
      contacts.removeAt(index);
      await saveContacts(contacts);
    }
  }

  // Update a contact by index
  Future<void> updateContact(int index, Map<String, String> updatedContact) async {
    final contacts = await loadContacts();
    if (index >= 0 && index < contacts.length) {
      contacts[index] = updatedContact;
      await saveContacts(contacts);
    }
  }

  // Clear all contacts
  Future<void> clearAllContacts() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove(_contactsKey);
  }
}