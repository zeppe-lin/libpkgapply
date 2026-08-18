// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/scripted_journal_store.h"

#include <stdexcept>
#include <utility>

namespace pkgapply::test {

std::string
scripted_journal_store::key(
    const application_journal_declaration_identity& identity)
{
  return identity.string();
}

application_journal_declaration
scripted_journal_store::publish_declaration(
    const application_journal_declaration& declaration)
{
  ++declaration_publications_;
  const std::string name = key(declaration.identity());
  const auto found = journals_.find(name);
  if (found != journals_.end()) {
    if (found->second.declaration.identity() != declaration.identity())
      throw std::logic_error("scripted journal declaration changed");
    latest_declaration_ = declaration.identity();
    return found->second.declaration;
  }
  auto inserted = journals_.emplace(
      name, journal_state{declaration, {}, std::nullopt});
  latest_declaration_ = declaration.identity();
  return inserted.first->second.declaration;
}

application_journal_step
scripted_journal_store::publish_step(const application_journal_step& step)
{
  ++step_publications_;
  const auto journal = journals_.find(key(step.declaration()));
  if (journal == journals_.end())
    throw std::logic_error("scripted journal step lacks declaration");
  const auto found = journal->second.steps.find(step.sequence());
  if (found != journal->second.steps.end()) {
    if (found->second.identity() != step.identity())
      throw std::logic_error("scripted journal step conflicts");
    return found->second;
  }
  return journal->second.steps.emplace(step.sequence(), step).first->second;
}

application_journal_cursor
scripted_journal_store::compare_and_publish_cursor(
    const std::optional<application_journal_cursor_identity>& expected,
    const application_journal_cursor& cursor)
{
  ++cursor_publications_;
  const auto journal = journals_.find(key(cursor.declaration()));
  if (journal == journals_.end())
    throw std::logic_error("scripted journal cursor lacks declaration");
  const auto current = journal->second.cursor
      ? std::optional(journal->second.cursor->identity())
      : std::nullopt;
  if (current != expected)
    throw std::logic_error("scripted journal cursor compare failed");
  journal->second.cursor = cursor;
  return *journal->second.cursor;
}

std::optional<application_journal_declaration>
scripted_journal_store::load_declaration(
    const application_journal_declaration_identity& identity)
{
  const auto found = journals_.find(key(identity));
  return found == journals_.end()
      ? std::nullopt
      : std::optional(found->second.declaration);
}

std::optional<application_journal_cursor>
scripted_journal_store::load_cursor(
    const application_journal_declaration_identity& declaration)
{
  const auto found = journals_.find(key(declaration));
  return found == journals_.end() ? std::nullopt : found->second.cursor;
}

std::optional<application_journal_step>
scripted_journal_store::load_step(
    const application_journal_declaration_identity& declaration,
    std::uint64_t sequence)
{
  const auto journal = journals_.find(key(declaration));
  if (journal == journals_.end())
    return std::nullopt;
  const auto found = journal->second.steps.find(sequence);
  return found == journal->second.steps.end()
      ? std::nullopt
      : std::optional(found->second);
}

std::size_t scripted_journal_store::declaration_publications() const noexcept
{
  return declaration_publications_;
}

std::size_t scripted_journal_store::step_publications() const noexcept
{
  return step_publications_;
}

std::size_t scripted_journal_store::cursor_publications() const noexcept
{
  return cursor_publications_;
}

const std::optional<application_journal_declaration_identity>&
scripted_journal_store::latest_declaration() const noexcept
{
  return latest_declaration_;
}


std::optional<application_journal_record>
scripted_journal_store::latest_snapshot() const
{
  if (!latest_declaration_)
    return std::nullopt;
  const auto journal = journals_.find(key(*latest_declaration_));
  if (journal == journals_.end() || !journal->second.cursor)
    return std::nullopt;
  std::vector<application_journal_event> events;
  events.reserve(journal->second.steps.size());
  for (std::uint64_t sequence = 0;
       sequence < journal->second.cursor->step_count(); ++sequence) {
    const auto step = journal->second.steps.find(sequence);
    if (step == journal->second.steps.end())
      throw std::logic_error("scripted journal committed step is missing");
    if (step->second.event())
      events.push_back(*step->second.event());
  }
  return application_journal_record::make(
      journal->second.declaration.header(), journal->second.cursor->state(),
      journal->second.declaration.effects(), std::move(events),
      journal->second.cursor->receipt(),
      journal->second.cursor->completed_evidence());
}

void scripted_journal_store::clear_counts() noexcept
{
  declaration_publications_ = 0;
  step_publications_ = 0;
  cursor_publications_ = 0;
}

} // namespace pkgapply::test
