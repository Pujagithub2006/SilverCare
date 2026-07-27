import React from 'react';
import { Routes, Route, Navigate } from 'react-router-dom';
import GuardianAuthPage from './pages/GuardianAuthPage';
import GuardianDashboardPage from './pages/GuardianDashboardPage';

export default function App() {
  const isGuardianLoggedIn = !!localStorage.getItem('guardian_username');

  return (
    <Routes>
      <Route
        path="/"
        element={
          isGuardianLoggedIn ? (
            <Navigate to="/guardian-dashboard" replace />
          ) : (
            <Navigate to="/guardian-auth" replace />
          )
        }
      />
      <Route path="/guardian-auth" element={<GuardianAuthPage />} />
      <Route path="/guardian-login" element={<GuardianAuthPage />} />
      <Route path="/guardian-dashboard" element={<GuardianDashboardPage />} />
      <Route path="/portal" element={<Navigate to="/guardian-auth" replace />} />
      <Route
        path="*"
        element={
          isGuardianLoggedIn ? (
            <Navigate to="/guardian-dashboard" replace />
          ) : (
            <Navigate to="/guardian-auth" replace />
          )
        }
      />
    </Routes>
  );
}
